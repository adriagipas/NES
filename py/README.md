# Mòdul Python per a depurar

Aquest mòdul fa una implementació bàsica del simulador utilitzant
Python, SDL1.2 i OpenGL amb l'objectiu de poder depurar el simulador:

- Pantalla amb resolució original
- Controls "hardcodejats":
  - UP: Amunt
  - DOWN: Avall
  - LEFT: Esquerra
  - RIGHT: Dreta
  - B: Z
  - A: X
  - START: Espai
  - SELECT: Retorn
- No es desa l'estat
- **CTRL-Q** per a eixir.
- **CTRL-R** per a reset.

Per a instal·lar el mòdul

```
pip install .
```

Un exemple bàsic d'ús es pot trobar en **exemple.py**:
```
python3 exemple.py ROM.ines
```

En la carpeta **debug** hi ha un script utilitzat per a depurar.
