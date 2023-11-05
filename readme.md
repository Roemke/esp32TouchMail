ESP32 per touch event aus dem DeepSleep wecken und Mail versenden.
Versenden geht, touch/deep sleep noch seltsam - nein, lag am ESP
geht auch.

Also: 
 1. Arduino Board mit esp32 - geht nicht 
 2. esp32 D1 Mini Board - geht
 3. firebeetle esp32-e - geht erst nicht, alles (Netz / Mail / elegantOTA) heraus und es geht. Alles wieder herein - es geht weiter. Sehr seltsam

Mal mit FastLED die LED blau zum leuchten gebracht, wenn er da ist. Dann sieht man wenigstens ob er in den deep sleep geht, 
blau aus - er schläft, was er dann braucht sehen wir dann, angeblich ca 10 uA, das wäre schon wenig.

Ist USB und ein 3.7 Akku angeschlossen, dann leuchtet die Spannung-LED rot (vermute er lädt den Akku), ist nur der Akku angeschlossen, dann ist alles aus. 

Bei den nicht funktionierenden Beispielen war es so, dass der deepsleep zwar aufgerufen wurde, der Wakeup aber sofort mit dem Grund 0 kam, also nicht von deep sleep aufgeweckt. 

Interessant - wenn er nicht am USB hängt, kommt der wakeup event nicht, also doch mal einen websocket einbauen, um Meldungen an den Webclient heraus zu geben

Die touch-Events sehen dann auch wie folgt aus:
56 56 56 56 56 56 56 56 56 56 56 56 56 56 56 56 56 56 54 56 
56 56 55 48 46 46 46 47 47 46 46 47 46 47 46 55 55 55 55 55 
55 47 47 46 47 47 46 47 46 47 46 47 47 46 47 55 46 46 46 46 
46 55 55 47 47 46 55 56 56 56 56 56 56 56 56 55 56 56 56 56 56 56 
schwankt also nur so um 10 herum, wenn man den threshold auf 50 setzt muesste es also gehen?
Tut es auch, hatte den falschen Wert gespeichert :-( (treshold statt threshold)...
Dennoch überraschend: bleibt das USB-Kabel ohne Spannung hängen, so sind die Unterschiede deutlich gravierender
noch ein wenig web sockets erweitert

