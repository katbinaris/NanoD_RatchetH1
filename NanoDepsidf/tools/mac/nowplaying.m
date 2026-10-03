// Now Playing for quadrad, straight from MediaRemote -- the source behind Control Center's Now
// Playing -- so it sees any player (Kaset, Music, Spotify, a browser tab) and its artwork with no
// Full Disk Access or Automation prompt. Since macOS 15.4 mediaremoted only answers Apple-signed
// processes, so this library doesn't run on its own: quadrad has /usr/bin/perl load it and call
// quadra_np_stream(), which never returns (the trick ungive/mediaremote-adapter uses).
//
// stdout, one JSON line per change:
//   {"bundle": "com.apple.Music", "playing": true, "title": "..", "artist": "..", "album": "..",
//    "art_seq": 3, "art": "<base64>" | null}
// "art" only comes along when art_seq moves (the artwork changed); null = there is none.
//
// Built by install.py: clang -dynamiclib -fobjc-arc -O2 -framework Foundation nowplaying.m

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

static void (*get_info)(dispatch_queue_t, void (^)(CFDictionaryRef));
static void (*get_playing)(dispatch_queue_t, void (^)(Boolean));
static void (*get_client)(dispatch_queue_t, void (^)(id));
static CFStringRef (*client_bundle)(id);
static CFStringRef (*client_parent)(id);
static void (*register_notes)(dispatch_queue_t);

static dispatch_queue_t s_q;        // MediaRemote calls back here
static dispatch_semaphore_t s_wake; // a Now Playing notification arrived

static BOOL wait_for(dispatch_semaphore_t done) {
    return dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC)) == 0;
}

static NSDictionary *now_info(void) {
    __block NSDictionary *out = nil;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    get_info(s_q, ^(CFDictionaryRef d) {
        out = d ? [(__bridge NSDictionary *)d copy] : @{};
        dispatch_semaphore_signal(done);
    });
    return wait_for(done) ? out : nil;
}

// The app, not its helper process: Kaset rather than com.apple.WebKit.GPU.
static NSString *now_bundle(void) {
    __block NSString *out = nil;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    get_client(s_q, ^(id client) {
        if (client) {
            CFStringRef s = client_parent ? client_parent(client) : NULL;
            if (!s || !CFStringGetLength(s)) s = client_bundle(client);
            out = s ? [(__bridge NSString *)s copy] : nil;
        }
        dispatch_semaphore_signal(done);
    });
    return wait_for(done) ? out : nil;
}

static int now_playing(void) { // 1, 0, or -1 = unknown
    if (!get_playing) return -1;
    __block int out = -1;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    get_playing(s_q, ^(Boolean playing) {
        out = playing ? 1 : 0;
        dispatch_semaphore_signal(done);
    });
    return wait_for(done) ? out : -1;
}

static NSString *text(NSDictionary *info, NSString *key) {
    id v = info[key];
    return [v isKindOfClass:NSString.class] ? v : @"";
}

static void emit(NSDictionary *line) {
    NSData *json = [NSJSONSerialization dataWithJSONObject:line options:0 error:NULL];
    if (!json) return;
    fwrite(json.bytes, 1, json.length, stdout);
    fputc('\n', stdout);
    fflush(stdout); // quadrad gone: SIGPIPE ends us
}

static void stream(void) {
    NSDictionary *last = nil;
    NSData *art = nil;
    long seq = 0;
    for (;;) {
        @autoreleasepool {
            if (getppid() == 1) exit(0); // quadrad is gone
            NSDictionary *info = now_info();
            if (info) {
                NSData *a = info[@"kMRMediaRemoteNowPlayingInfoArtworkData"];
                if (![a isKindOfClass:NSData.class] || a.length == 0) a = nil;
                if (a != art && ![a isEqualToData:art]) {
                    art = a;
                    seq++;
                }
                int playing = info.count ? now_playing() : 0;
                if (playing < 0) {
                    id rate = info[@"kMRMediaRemoteNowPlayingInfoPlaybackRate"];
                    playing = [rate isKindOfClass:NSNumber.class] && [rate doubleValue] > 0;
                }
                NSString *bundle = info.count ? now_bundle() : nil;
                NSDictionary *state = @{
                    @"bundle" : bundle ?: (id)NSNull.null,
                    @"playing" : playing == 1 ? @YES : @NO,
                    @"title" : text(info, @"kMRMediaRemoteNowPlayingInfoTitle"),
                    @"artist" : text(info, @"kMRMediaRemoteNowPlayingInfoArtist"),
                    @"album" : text(info, @"kMRMediaRemoteNowPlayingInfoAlbum"),
                    @"art_seq" : @(seq),
                };
                if (![state isEqual:last]) {
                    NSMutableDictionary *line = [state mutableCopy];
                    if (!last || ![last[@"art_seq"] isEqual:state[@"art_seq"]])
                        line[@"art"] = art ? [art base64EncodedStringWithOptions:0] : (id)NSNull.null;
                    emit(line);
                    last = state;
                }
            }
        }
        // A notification, or 2 s (they aren't guaranteed). A track change brings a burst (title,
        // artwork, rate): let it settle into one snapshot.
        if (dispatch_semaphore_wait(s_wake, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) == 0) {
            usleep(150 * 1000);
            while (dispatch_semaphore_wait(s_wake, DISPATCH_TIME_NOW) == 0) {
            }
        }
    }
}

__attribute__((visibility("default"))) void quadra_np_stream(void) {
    void *mr = dlopen("/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote", RTLD_NOW);
    if (mr) {
        get_info = dlsym(mr, "MRMediaRemoteGetNowPlayingInfo");
        get_playing = dlsym(mr, "MRMediaRemoteGetNowPlayingApplicationIsPlaying");
        get_client = dlsym(mr, "MRMediaRemoteGetNowPlayingClient");
        client_bundle = dlsym(mr, "MRNowPlayingClientGetBundleIdentifier");
        client_parent = dlsym(mr, "MRNowPlayingClientGetParentAppBundleIdentifier");
        register_notes = dlsym(mr, "MRMediaRemoteRegisterForNowPlayingNotifications");
    }
    if (!get_info || !get_client || !client_bundle) {
        fputs("nowplaying: this macOS has no MediaRemote Now Playing API\n", stderr);
        exit(3);
    }
    s_q = dispatch_queue_create("quadra.nowplaying", DISPATCH_QUEUE_SERIAL);
    s_wake = dispatch_semaphore_create(0);
    if (register_notes) {
        register_notes(s_q);
        const char *names[] = {"kMRMediaRemoteNowPlayingInfoDidChangeNotification",
                               "kMRMediaRemoteNowPlayingApplicationIsPlayingDidChangeNotification",
                               "kMRMediaRemoteNowPlayingApplicationDidChangeNotification"};
        for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
            CFStringRef *name = dlsym(mr, names[i]);
            if (name && *name)
                [NSNotificationCenter.defaultCenter addObserverForName:(__bridge NSString *)*name
                                                                object:nil
                                                                 queue:nil
                                                            usingBlock:^(NSNotification *n) {
                                                                dispatch_semaphore_signal(s_wake);
                                                            }];
        }
    }
    [NSThread detachNewThreadWithBlock:^{
        stream();
    }];
    dispatch_main(); // MediaRemote may call back on the main queue: keep it serviced
}
