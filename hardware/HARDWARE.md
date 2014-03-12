The hardware for the irrigation controller consists of a central 'plugboard' which contains the
RJ11 connectors for the standard 1-wire connectors from the 1-wire master and the temperature sensors.

It also contains the ribbon connectors for the zone cards, each of which handles 8 irrigation valves.

The precision rectifier for the current transformer is also on the plugboard, along with the 12v power 
connection for the zone cards.


The zone cards each handle 8 valves and contain 4 off DS2413 dual channel 1-wire switches and 8 triacs
and indicator LEDs.

The PCB layout is optimised for debugging a prototype rather then anything else, the current transformer 
is similarly a prototype, it being a stripped out wall wart stransformer with the seconardy removed
and replaced by 4 turns of wire as a primary to measure the current from the 24volt transformer that
powers the valves.

The power supply for the zone cards and the plugboard is taken from the 12v supply for the router
that provides the CPU!!




