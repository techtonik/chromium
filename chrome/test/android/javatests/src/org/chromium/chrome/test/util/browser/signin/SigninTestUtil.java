// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.preference.PreferenceManager;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.signin.AccountIdProvider;
import org.chromium.chrome.browser.signin.AccountTrackerService;
import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.AccountHolder;
import org.chromium.sync.test.util.MockAccountManager;

import java.util.HashSet;

/**
 * Utility class for test signin functionality.
 */
public final class SigninTestUtil {
    private static final String TAG = "cr.Signin";

    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    private static SigninTestUtil sInstance;

    private Context mContext;
    private MockAccountManager mAccountManager;

    /**
     * Sets up the test authentication environment.
     *
     * This must be called before native is loaded.
     */
    public static void setUpAuthForTest(Instrumentation instrumentation) {
        assert sInstance == null;
        sInstance = new SigninTestUtil(instrumentation);
    }

    /**
     * Get the object created in setUpAuthForTest.
     */
    public static SigninTestUtil get() {
        assert sInstance != null;
        return sInstance;
    }

    private SigninTestUtil(Instrumentation instrumentation) {
        mContext = instrumentation.getTargetContext();
        mAccountManager = new MockAccountManager(mContext, instrumentation.getContext());
        AccountManagerHelper.overrideAccountManagerHelperForTests(mContext, mAccountManager);
        overrideAccountIdProvider();
        // Clear cached signed account name here or tests can flake.
        ChromeSigninController.get(mContext).setSignedInAccountName(null);
        // Clear cached accounts list here or tests can also flake. Needs to use the app context.
        PreferenceManager.getDefaultSharedPreferences(mContext.getApplicationContext())
                .edit()
                .putStringSet(OAuth2TokenService.STORED_ACCOUNTS_KEY, new HashSet<String>())
                .apply();
    }

    /**
     * Add an account with the default name.
     */
    public Account addTestAccount() {
        Account account = createTestAccount(DEFAULT_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountTrackerService.get(mContext).forceRefresh();
            }
        });
        return account;
    }

    /**
     * Add and sign in an account with the default name.
     */
    public Account addAndSignInTestAccount() {
        Account account = createTestAccount(DEFAULT_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeSigninController.get(mContext).setSignedInAccountName(DEFAULT_ACCOUNT);
                AccountTrackerService.get(mContext).forceRefresh();
            }
        });
        return account;
    }

    private Account createTestAccount(String accountName) {
        Account account = AccountManagerHelper.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder =
                AccountHolder.create().account(account).alwaysAccept(true);
        mAccountManager.addAccountHolderExplicitly(accountHolder.build());
        return account;
    }

    private void overrideAccountIdProvider() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountIdProvider.setInstanceForTest(new AccountIdProvider() {
                    @Override
                    public String getAccountId(Context ctx, String accountName) {
                        return "gaia-id-" + accountName;
                    }

                    @Override
                    public boolean canBeUsed(Context ctx, Activity activity) {
                        return true;
                    }
                });
            }
        });
    }
}
