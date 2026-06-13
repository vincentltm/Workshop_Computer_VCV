# Utility Pair (single card)
Single card version of the [Utility Pair](https://github.com/chrisgjohnson/Utility-Pair) cards for the Music Thing Modular Workshop System Computer.

### Code
Most of the code for this is essentially a duplicate of the individual Utility Pair code. The key differences are:
- Loop divider removed
- Utility selection menu at bootup
- A custom version of TalkiePCM, used for LPC-encoded speech in the selection menu

Two python scripts are included:
- `make_selections.py`: very hacky code generation to produce the numerous card pairings at runtime. This would be a crazy way of doing things, were I not trying to do a quick modification of the existing Utility Pair code.
- `audio/generate_audio.py`: script I used to generate the card names audio data. This has a lot of paths hard-coded and will need modification to run on anyone elses computer.

## Audio generation
The pipeline for audio generation by the `audio/generate_audio.py` script is
- Piper TTS-generated audio, using the Piper TTS 'Alan' and [Dioco Jenny](https://github.com/dioco-group/jenny-tts-dataset) voices, at roughly half speed.
- Compression to TMS5220 LPC encoding using [python_wizard](https://github.com/ptwz/python_wizard)
- Playback at 2× speed using a custom version of [TalkiePCM](https://github.com/pschatzmann/TalkiePCM), which interpolates between settings.

Generating audio at half speed, and playing back a double speed, doubles the data rate of the LPC coding, with corresponding increase in quality.
