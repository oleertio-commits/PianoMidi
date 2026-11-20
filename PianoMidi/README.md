# PianoMidi (VST3, Windows)

Plugin VST3 que convierte audio polifonico (piano) en notas MIDI en tiempo real.

## Que hace
- Analiza el audio de la pista con FFT de 16384 puntos y deteccion polifonica por suma armonica.
- Emite notas MIDI (Note On / Note Off con velocidad) por la salida MIDI del plugin.
- Graba lo detectado y lo exporta a `.mid` (boton **Guardar .mid** o arrastrando el bloque a una pista).

## Controles
Umbral, Voces (polifonia maxima), Nota grave / Nota aguda (rango), Canal MIDI, Audio directo.

## Compilar en GitHub (automatico)
1. Crea un repositorio vacio en GitHub, p. ej. `PianoMidi`.
2. En esta carpeta:
   ```
   git init
   git add .
   git commit -m "PianoMidi v1.0"
   git branch -M main
   git remote add origin https://github.com/TU_USUARIO/PianoMidi.git
   git push -u origin main
   ```
3. Ve a la pestana **Actions**: el flujo "Build PianoMidi VST3 (Windows)" compila solo.
   Descarga `PianoMidi-VST3-Windows` desde *Artifacts*.
4. Para publicar una release descargable: `git tag v1.0.0 && git push origin v1.0.0`.

## Instalar
Copia la carpeta `PianoMidi.vst3` a `C:\Program Files\Common Files\VST3\` y reescanea plugins en Cakewalk.

## Compilar en local (opcional)
Requiere Visual Studio 2022 (carga "Desarrollo para escritorio con C++") y CMake:
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Resultado: `build/PianoMidi_artefacts/Release/VST3/PianoMidi.vst3`
