## TODO
[x] Buscar cómo hacer un secuenciador
[] aplicar formula euclidiana
[] multi-track MIDI out (4+ voices) 
[] tempo sync // Sync to transport using AudioPlayHead for tempo/position in plugins
[] Basic velocity Modulation

## Spring 1: se emitenn seniales MIDI
[] golbla tempo sync -> Sync usando AudioPlayHead para obtener el tempo y la posición en el transport (L164 @PluginProcessor.cpp)
[] ritmo hardcodeado (A) -> 16 pasos, 8 hits, rotación 0 (L71 @PluginProcessor.h)
[] 1 nota, 1 canal -> C4 (L215 @PluginProcessor.cpp)

## Spring 2: se crean parámetros
[] Crear parametros Steps, hits, rot, nota
[] agregar Algo euclidiano

## Spring 3
[] Armar UI basica
[] linkear parámetro a la UI

## Spring 4
[] agregar ritmo B

## Spring 5
[] Agregar velocity mod