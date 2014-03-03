
json items used by UI in html file

"zone":             int        index to a specific item
"id":               alphanum   used for diagnostics, etc
"x":                int        % (0-(100-w)) of distance from left of image map
"y":                int        % (0-(100-h)) of distance down from top of image map
"w":                int        % (0-100) width of box drawn no image map
"h":                int        % (0-100) height of box drawn on image map
"zoom":             int        optional zoom factor. Determines whether to try and show name of the zone
"selectable":       bool       if the user can select it for programming
"name":             alphanum   plain text displayable name


"zone":  8
"id": "tunnel"
"aorb": true
"address": "3a.2d3201000000"
"flow": 8
"current": 160

"isdpfeed":
"ispump":
"isfrost":
"isdpfeed":
"istest":
"isspare":

"maxflow":
"maxstarts":
"start": 0700
"end": 2100
"group"


"zone":  8
"id": "tunnel"
"x": 82
"y": 42
"w":  2
"h":  6
"zoom": 13
"selectable": true
"name": "Tunnel House"
"aorb": true
"address": "3a.2d3201000000"
"flow": 8
"current": 160



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
"isdpfeed": true
"start": 0700
"end": 2100
"current": 160


"zone": 40
"id": "allnuts"
"x": 87"y": 5
"w": 12
"h": 10
"zoom": 3
"selectable": true
"name": "All Walnuts"
"group": 1
"zones": [12345]



"zone": 43
"id": "test"
"x": 87
"y": 92
"w": 12
"h":  5
"zoom": 3
"selectable": true
"name": "Test Valves"
"istest": true



