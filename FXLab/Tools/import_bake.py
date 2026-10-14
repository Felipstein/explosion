"""Importa só um cozimento (as três folhas) em /Game/FX/<cozimento>, sem remontar o mapa.
Uso: fxlab.ps1 import -Name <cozimento>  (Unreal sem janela; a variável FXLAB_IMPORT_BAKE leva o nome)"""
import builtins
import os
import runpy

builtins.FXLAB_NO_BUILD = True
ns = runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'setup_fxlab.py'))
ns['import_bake'](os.environ['FXLAB_IMPORT_BAKE'])
