addSubdirectory("audio")
addSubdirectory("gamepad")
addSubdirectory("mediastream")

pageBundle = Library:new("PageBundle")
pageBundle:usePackage(nix)
pageBundle:useTarget(audio)
pageBundle:useTarget(gamepad)
pageBundle:useTarget(usermedia)

pageBundle:addFiles([[
    PageBundle.cpp
    PlatformClient.cpp
]])
