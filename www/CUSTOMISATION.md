
json items used by UI in html file
```
"zone":             int        index to a specific item
"id":               alphanum   used for diagnostics, etc
"x":                int        % (0-(100-w)) of distance from left of image map
"y":                int        % (0-(100-h)) of distance down from top of image map
"w":                int        % (0-100) width of box drawn no image map
"h":                int        % (0-100) height of box drawn on image map
"selectable":       bool       if the user can select it for programming
"name":             alphanum   plain text displayable name
```

json items used for normal programable zones
```
"zone":             int        index to a specific item
"id":               alphanum   used for diagnostics, etc
"aorb":             bool       indicates channel A or B on the DS2413 driver
"address":          hexstring  <family>.<serialnumber> 1-wire address
"flow":             int        flow rate in litres/minute
"current":          int        current draw of valve ni milliamps
```

json boolean items that modify a zone
```
"isdpfeed":         bool       domestic feed
"ispump":           bool       seperate pump
"isfrost":          bool       frost protection applies
"istest":           bool       is a virtual zone that trigger test mode
"isspare":          bool       not used
```

json items that pass useful information about a zone
```
"maxflow":          int        maximum flow of a pump in litres/minute
"maxstarts":        int        maximum number of starts of a pump allowed per hour
"start":            int        time in hhmm format of when to start domestic feed for stock
"end":              int        time in hhmm format of when to stop domestic feed
"group"             int        identifier for a group from 1-15
"zones"             array      list of zones in a group
```
experimental items
```
"zoom":             int        optional zoom factor. Determines whether to try and show name of the zone
                               doessn't work with image maps very well!!
```


Examples


A small zone that runs the misters in a tunnel house
```
"zone":  8                       // arbitrary number from 1 to MAXZONES
"id": "tunnel"                   // label used for debugging
"x": 82                          // how far across from top left
"y": 42                          // how far down
"w":  2                          // quite narrow!!
"h":  6                          // tall
"zoom": 13                       // values > 10 dipsplay the whole name in the box
"selectable": true               // can be selected to program
"name": "Tunnel House"           // friendly name
"aorb": true                     // PIO.A
"address": "3a.2d3201000000"     // owfs format 1-wire address
"flow": 8                        // 8 l/m flow rate
"current": 160                   // draws 160mA
```


A zone that describes the valve that allows the domestic feed to pressurise the irrigation system to run
stock troughs and taps about the place. It also handles low volume irrigation up to the flow rate of the main pump.
```
"zone": 32
"id": "dpfeed"
"x": 62
"y": 65
"w":  2
"h":  2
"zoom": 13
"selectable": false
"name": "Domestic Feed"
"aorb": false
"address": "3a.9b8d03000000"
"flow": 0
"isdpfeed": true                 // tell the system its special
"start": 0700                    // even if no demand then turn on at 7:00am anyway
"end": 2100                      // if no demand the turn off at night
"current": 160
```


A (virtual) zone that has no direct valves but instead holds a group of zones. It appears on the UI and can be programmed
(hence selectable) 
```
"zone": 40
"id": "allnuts"
"x": 87"y": 5
"w": 12
"h": 10
"zoom": 3
"selectable": true
"name": "All Walnuts"
"group": 1                       // a numeric 'handle' for the group (a zone can be in more than 1 group)
"zones": [1, 2, 3, 4, 5]         // list of zones in the group
```

Another virtual zone that has no valves but in this case operates o na specific internal function (test mode)
```
"zone": 43
"id": "test"
"x": 87
"y": 92
"w": 12
"h":  5
"zoom": 3
"selectable": true
"name": "Test Valves"
"istest": true                   // say what it does
```



