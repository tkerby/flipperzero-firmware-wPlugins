Generated game assets, written by tools/mapc.py / tools/mkfont.py and shipped with the app via
fap_file_assets="files":
  map_NN.bin                       maps
  lang/<xx>/font.nfnt              language font (tools/mkfont.py)
  lang/<xx>/kb.bin                 naming keyboard: grid pages + code <-> Unicode pairs (tools/mkfont.py, assets/lang/keyboard.toml, contract 11.6)
  lang/<xx>/txt_NN.bin, txt_gl.bin text tables per language (tools/mapc.py build)
  fx.bin                           battle effects: the baked animations the battle plugin streams (tools/fxbake, contract 10.4)
