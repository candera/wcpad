TODO: 

* Add check for convexity of quadrangles: sometimes "boomerangs" get tracked
* Update tracking to follow quadrangles that have only three corners or sides visible
* How does gathering more noise data affect finger detection? 
* Would more smoothing help? 
* What about a L2 metric between color data? 
* Track border corners 
* Map bordered region to a new 100x100 image 
* Why is CPU so high? Anything I can do to lower it? 
* Detect image of hand purely as difference against background

DEFER: 
* Try tracking corner points - what happens when corner is obscured? And then comes back? 

DONE: 
* Try to repro crash that happened when I left the app running for a long time
* Update test square to have vx & vx components: 
* Detect border via single-frame analysis of straight lines (doesn't quite work)
  * Find contours
  * Interpolate as polygons
  * Discard segments smaller than a threshold
  * Merge colinear segments
  * Find intersection points
  * Compute all quadrangles based on intersection points
  * Discard quadrangles that don't meet search criteria
  * Should allow for multiple quadrangle detection
* Fix memory leak
* Do other color spaces give better differentiation of fingers? 
  * Yes. XYZ seems to be the best, at least for my hands. 
