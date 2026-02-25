##


## Spring 1: se emitenn seniales MIDI
[x] global tempo sync -> Sync usando AudioPlayHead para obtener el tempo y la posición en el transport (L164 @PluginProcessor.cpp)
[x] ritmo hardcodeado (A) -> 16 pasos, 8 hits, rotación 0 (L71 @PluginProcessor.h)
[x] 1 nota, 1 canal -> C4 (L215 @PluginProcessor.cpp)

## Spring 2: se crean parámetros
[x] Crear parametros 
         Steps: int, [0-32]
         hits: int, [0-Steps]
         rot: int, [0-Steps-1]
         nota: int, [0-127]
[x] agregar Algo euclidiano 
[] grabar parámetros y levantarlos al reiniciar         
[] validación de parámetros


## Spring 3
[] Armar UI basica
[] linkear parámetro a la UI

## Spring 4
[] agregar ritmo B

## Spring 5
[] Agregar velocity mod


Averiguar cómo acelerar la compilación

