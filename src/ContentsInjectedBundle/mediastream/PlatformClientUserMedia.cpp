#include "PlatformClient.h"
#include "MediaStreamCenter.h"
#include "UserMediaClient.h"

Nix::MediaStreamCenter* PlatformClient::createMediaStreamCenter()
{
    return new MediaStreamCenter();
}

Nix::UserMediaClient* PlatformClient::createUserMediaClient()
{
    return new UserMediaClient();
}
