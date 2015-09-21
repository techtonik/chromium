// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Each |DataUsageBucket| corresponds to data usage for an interval of
// |kDataUsageBucketLengthMins| minutes. We store data usage for the past
// |kNumDataUsageBuckets| buckets. Buckets are maintained as a circular array
// with indexes from 0 to (kNumDataUsageBuckets - 1). To store the circular
// array in a key-value store, we convert each index to a unique key. The latest
// bucket persisted to DB overwrites the oldest.

#include "components/data_reduction_proxy/core/browser/data_usage_store.h"

#include <stdlib.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"

namespace {
const char kCurrentBucketIndexKey[] = "current_bucket_index";
const char kBucketKeyPrefix[] = "data_usage_bucket:";

const int kMinutesInHour = 60;
const int kMinutesInDay = 24 * kMinutesInHour;

// Time interval for each DataUsageBucket.
const int kDataUsageBucketLengthInMinutes = 5;
static_assert(kDataUsageBucketLengthInMinutes > 0,
              "Length of time should be positive");
static_assert(kMinutesInHour % kDataUsageBucketLengthInMinutes == 0,
              "kDataUsageBucketLengthMins must be a factor of kMinsInHour");

// Number of days for which to maintain data usage history.
const int kDataUsageHistoryNumDays = 60;

// Total number of buckets persisted to DB.
const int kNumDataUsageBuckets =
    kDataUsageHistoryNumDays * kMinutesInDay / kDataUsageBucketLengthInMinutes;

std::string DbKeyForBucketIndex(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kNumDataUsageBuckets);

  return base::StringPrintf("%s%d", kBucketKeyPrefix, index);
}

base::Time BucketLowerBoundary(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.minute -= exploded.minute % kDataUsageBucketLengthInMinutes;
  exploded.second = 0;
  exploded.millisecond = 0;
  return base::Time::FromUTCExploded(exploded);
}

}  // namespace

namespace data_reduction_proxy {

DataUsageStore::DataUsageStore(DataStore* db)
    : db_(db), current_bucket_index_(-1) {
  sequence_checker_.DetachFromSequence();
}

DataUsageStore::~DataUsageStore() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

void DataUsageStore::LoadDataUsage(std::vector<DataUsageBucket>* data_usage) {
  DCHECK(data_usage);
  DataUsageBucket empty_bucket;
  data_usage->clear();
  data_usage->resize(kNumDataUsageBuckets, empty_bucket);

  for (int i = 0; i < kNumDataUsageBuckets; ++i) {
    int circular_array_index =
        (i + current_bucket_index_ + 1) % kNumDataUsageBuckets;
    LoadBucketAtIndex(circular_array_index, &data_usage->at(i));
  }
}

void DataUsageStore::LoadCurrentDataUsageBucket(DataUsageBucket* current) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(current);

  std::string current_index_string;
  DataStore::Status index_read_status =
      db_->Get(kCurrentBucketIndexKey, &current_index_string);

  if (index_read_status == DataStore::Status::OK)
    current_bucket_index_ = atoi(current_index_string.c_str());
  else
    current_bucket_index_ = 0;

  DCHECK_GE(current_bucket_index_, 0);
  DCHECK_LT(current_bucket_index_, kNumDataUsageBuckets);

  DataStore::Status status = LoadBucketAtIndex(current_bucket_index_, current);
  if (status == DataStore::Status::OK) {
    current_bucket_last_updated_ =
        base::Time::FromInternalValue(current->last_updated_timestamp());
  }
}

void DataUsageStore::StoreCurrentDataUsageBucket(
    const DataUsageBucket& current) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(current_bucket_index_ >= 0 &&
         current_bucket_index_ < kNumDataUsageBuckets);

  // If current bucket does not have any information, we skip writing to DB.
  if (!current.has_last_updated_timestamp())
    return;

  int prev_current_bucket_index = current_bucket_index_;
  base::Time prev_current_bucket_last_updated_ = current_bucket_last_updated_;
  // We might have skipped saving buckets because Chrome was not being used.
  // Write empty buckets to those slots to overwrite outdated information.
  base::Time last_updated =
      base::Time::FromInternalValue(current.last_updated_timestamp());
  std::map<std::string, std::string> buckets_to_save;
  int num_buckets_since_last_saved = BucketOffsetFromLastSaved(last_updated);
  DataUsageBucket empty_bucket;
  for (int i = 0; i < num_buckets_since_last_saved - 1; ++i)
    GenerateKeyAndAddToMap(empty_bucket, &buckets_to_save, true);

  GenerateKeyAndAddToMap(current, &buckets_to_save,
                         num_buckets_since_last_saved > 0);

  current_bucket_last_updated_ =
      base::Time::FromInternalValue(current.last_updated_timestamp());

  std::stringstream current_index_string;
  current_index_string << current_bucket_index_;
  buckets_to_save.insert(std::pair<std::string, std::string>(
      kCurrentBucketIndexKey, current_index_string.str()));

  DataStore::Status status = db_->Put(buckets_to_save);
  if (status != DataStore::Status::OK) {
    current_bucket_index_ = prev_current_bucket_index;
    current_bucket_last_updated_ = prev_current_bucket_last_updated_;
    LOG(WARNING) << "Failed to write data usage buckets to LevelDB" << status;
  }
}

// static
bool DataUsageStore::IsInCurrentInterval(const base::Time& time) {
  if (time.is_null())
    return true;

  return BucketLowerBoundary(base::Time::Now()) == BucketLowerBoundary(time);
}

void DataUsageStore::GenerateKeyAndAddToMap(
    const DataUsageBucket& bucket,
    std::map<std::string, std::string>* map,
    bool increment_current_index) {
  if (increment_current_index) {
    current_bucket_index_++;
    DCHECK(current_bucket_index_ <= kNumDataUsageBuckets);
    if (current_bucket_index_ == kNumDataUsageBuckets)
      current_bucket_index_ = 0;
  }

  std::string bucket_key = DbKeyForBucketIndex(current_bucket_index_);

  std::string bucket_value;
  bool success = bucket.SerializeToString(&bucket_value);
  DCHECK(success);

  map->insert(std::pair<std::string, std::string>(bucket_key, bucket_value));
}

int DataUsageStore::BucketOffsetFromLastSaved(
    const base::Time& new_last_updated_timestamp) const {
  if (current_bucket_last_updated_.is_null())
    return 0;

  base::TimeDelta time_delta =
      BucketLowerBoundary(new_last_updated_timestamp) -
      BucketLowerBoundary(current_bucket_last_updated_);
  int offset_from_last_saved =
      (time_delta.InMinutes() / kDataUsageBucketLengthInMinutes);
  return std::min(offset_from_last_saved, kNumDataUsageBuckets);
}

DataStore::Status DataUsageStore::LoadBucketAtIndex(int index,
                                                    DataUsageBucket* bucket) {
  DCHECK(index >= 0 && index < kNumDataUsageBuckets);

  std::string bucket_as_string;
  DataStore::Status bucket_read_status =
      db_->Get(DbKeyForBucketIndex(index), &bucket_as_string);
  if ((bucket_read_status != DataStore::Status::OK &&
       bucket_read_status != DataStore::NOT_FOUND)) {
    LOG(WARNING) << "Failed to read data usage bucket from LevelDB: "
                 << bucket_read_status;
  }
  if (bucket_read_status == DataStore::Status::OK) {
    bool parse_successful = bucket->ParseFromString(bucket_as_string);
    DCHECK(parse_successful);
  }
  return bucket_read_status;
}

}  // namespace data_reduction_proxy
