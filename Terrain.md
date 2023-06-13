```vega-lite
{
  "width": 500, "height": 200,
  "data": { "url" : "asset/error.json" },
  "mark": { 
    "type" : "line",
    "clip" : true,
    "interpolate" : "monotone" },
  "encoding": {
    "x": { "field": "geometry error", "type": "quantitative",
           "scale": {"domain": [0, 0.001] } },
    "y": { "field": "triangle", "type": "quantitative", 
           "scale" : { "type" : "log", "domain" : [512, 150000] }
    }
  }
}
```
