mediastream = Library:new("mediastream", STATIC)
mediastream:usePackage(nix)
mediastream:addIncludePath("..")
mediastream:addFiles([[
    UserMediaClient.cpp
    PlatformClientUserMedia.cpp
    MediaStreamCenter.cpp
]])
