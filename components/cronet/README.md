# Quick Start Guide to Using Cronet
Cronet is the networking stack of Chromium put into a library for use on
mobile. This is the same networking stack that is used in the Chrome browser
by over a billion people. It offers an easy-to-use, high performance,
standards-compliant, and secure way to perform HTTP requests. Cronet has support
for both Android and iOS. On Android, Cronet offers its own Java asynchronous
API as well as support for the [java.net.HttpURLConnection] API.
This document gives a brief introduction to using these two Java APIs.

### Basics
First you will need to extend `UrlRequestListener` to handle
events during the lifetime of a request. For example:

    class MyListener extends UrlRequestListener {
        @Override
        public void onReceivedRedirect(UrlRequest request,
                ResponseInfo responseInfo, String newLocationUrl) {
            if (followRedirect) {
                // Let's tell Cronet to follow the redirect!
                mRequest.followRedirect();
            } else {
                // Not worth following the redirect? Abandon the request.
                mRequest.cancel();
            }
        }

        @Override
        public void onResponseStarted(UrlRequest request,
                ResponseInfo responseInfo) {
             // Now we have response headers!
             int httpStatusCode = responseInfo.getHttpStatusCode();
             if (httpStatusCode == 200) {
                 // Success! Let's tell Cronet to read the response body.
                 request.read(myBuffer);
             } else if (httpStatusCode == 503) {
                 // Do something. Note that 4XX and 5XX are not considered
                 // errors from Cronet's perspective since the response is
                 // successfully read.
             }
             responseHeaders = responseInfo.getAllHeaders();
        }

        @Override
        public void onReadCompleted(UrlRequest request,
                ResponseInfo responseInfo, ByteBuffer byteBuffer) {
             // Response body is available.
             doSomethingWithResponseData(byteBuffer);
             // Let's tell Cronet to continue reading the response body or
             // inform us that the response is complete!
             request.read(myBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request,
                ExtendedResponseInfo extendedResponseInfo) {
             // Request has completed successfully!
        }

        @Override
        public void onFailed(UrlRequest request,
                ResponseInfo responseInfo, UrlRequestException error) {
             // Request has failed. responseInfo might be null.
             Log.e("MyListener", "Request failed. " + error.getMessage());
             // Maybe handle error here. Typical errors include hostname
             // not resolved, connection to server refused, etc.
        }
    }

Make a request like this:

    CronetEngine.Builder engineBuilder = new CronetEngine.Builder(getContext());
    CronetEngine engine = engineBuilder.build();
    Executor executor = Executors.newSingleThreadExecutor();
    MyListener listener = new MyListener();
    UrlRequest.Builder requestBuilder = new UrlRequest.Builder(
            "https://www.example.com", listener, executor, engine);
    UrlRequest request = requestBuilder.build();
    request.start();

In the above example, `MyListener` extends `UrlRequestListener`. The request
is started asynchronously. When the response is ready (fully or partially), and
in the event of failures or redirects, `listener`'s methods will be invoked on
`executor`'s thread to inform the client of the request state and/or response
information.

### Downloading Data
When Cronet fetches response headers from the server or gets them from the
cache, `UrlRequestListener.onResponseStarted` will be invoked. To read the
response body, the client should call `UrlRequest.read` and supply a
[ByteBuffer] for Cronet to fill. Once a portion or all of
the response body is read, `UrlRequestListener.onReadCompleted` will be invoked.
The client may then read and consume the data within `byteBuffer`.
Once the client is ready to consume more data, the client should call
`UrlRequest.read` again. The process continues until
`UrlRequestListener.onSucceeded` or `UrlRequestListener.onFailed` is invoked,
which signals the completion of the request.

### Uploading Data
    MyUploadDataProvider myUploadDataProvider = new MyUploadDataProvider();
    requestBuilder.setHttpMethod("POST");
    requestBuilder.setUploadDataProvider(myUploadDataProvider, executor);

In the above example, `MyUploadDataProvider` extends `UploadDataProvider`.
When Cronet is ready to send the request body,
`myUploadDataProvider.read(UploadDataSink uploadDataSink,
ByteBuffer byteBuffer)` will be invoked. The client will need to write the
request body into `byteBuffer`. Once the client is done writing into
`byteBuffer`, the client can let Cronet know by calling
`uploadDataSink.onReadSucceeded`. If the request body doesn't fit into
`byteBuffer`, the client can continue writing when `UploadDataProvider.read` is
invoked again. For more details, please see the API reference.

### <a id=configuring-cronet></a> Configuring Cronet
Various configuration options are available via the `CronetEngine.Builder`
object.

Enabling HTTP/2, QUIC, or SDCH:

- For Example:

        engineBuilder.enableSPDY(true).enableQUIC(true).enableSDCH(true);

Controlling the cache:

- Use a 100KiB in-memory cache:

        engineBuilder.enableHttpCache(
                CronetEngine.Builder.HttpCache.IN_MEMORY, 100 * 1024);

- or use a 1MiB disk cache:

        engineBuilder.setStoragePath(storagePathString);
        engineBuilder.enableHttpCache(CronetEngine.Builder.HttpCache.DISK,
                1024 * 1024);

### Debugging
To get more information about how Cronet is processing network
requests, you can start and stop **NetLog** logging by calling
`CronetEngine.startNetLogToFile` and `CronetEngine.stopNetLog`.
Bear in mind that logs may contain sensitive data. You may analyze the
generated log by navigating to [chrome://net-internals#import] using a
Chrome browser.

# Using the java.net.HttpURLConnection API
Cronet offers an implementation of the [java.net.HttpURLConnection] API to make
it easier for apps which rely on this API to use Cronet.
To open individual connections using Cronet's implementation, do the following:

    HttpURLConnection connection =
            (HttpURLConnection)engine.openConnection(url);

To use Cronet's implementation instead of the system's default implementation
for all connections established using `URL.openConnection()` do the following:

    URL.setURLStreamHandlerFactory(engine.createURLStreamHandlerFactory());

Cronet's
HttpURLConnection implementation has some limitations as compared to the system
implementation, including not utilizing the default system HTTP cache (Please
see {@link org.chromium.net.CronetEngine#createURLStreamHandlerFactory} for
more information).
You can configure Cronet and control caching through the
`CronetEngine.Builder` instance, `engineBuilder`
(See [Configuring Cronet](#configuring-cronet) section), before you build the
`CronetEngine` and then call `CronetEngine.createURLStreamHandlerFactory()`.

[ByteBuffer]: https://developer.android.com/reference/java/nio/ByteBuffer.html
[chrome://net-internals#import]: chrome://net-internals#import
[java.net.HttpURLConnection]: https://developer.android.com/reference/java/net/HttpURLConnection.html
