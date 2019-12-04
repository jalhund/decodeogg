# DecodeOgg

Native extension for decoding ogg files in wav.

The example also uses [OpenAl](https://github.com/Lerg/extension-openal)
## Using

```lua
local res = resource.load("/assets/sound.ogg")
--.ogg in res
decodeogg.decodeogg(res)
--Now .wav in res
```

## Setup


Open `game.project` and in the dependencies field (in the project section) add:

https://github.com/JAlHund/DecodeOgg/archive/master.zip

Specify .ogg resources directory in `game.project`.

## Example Audio File
Audio file Take the Lead by Kevin Macleod. Licensed under the Creative Commons Attribution 3.0 Unported license. 