TODO: 

* Update software to track single border and fingertip in main loop
* Repackage app as a DLL that can report results to a C# app
* Map bordered region to a new 100x100 image 
* Why is CPU so high? Anything I can do to lower it? 

DEFER: 
* Track border corners 
* How does gathering more noise data affect finger detection? 
* Try using the OpenCV tracking features again
* See what effect using white/black boundaries has on detection
* Try tracking corner points - what happens when corner is obscured? And then comes back? 
* Would more smoothing help? 
* Detect image of hand purely as difference against background

DISCARD: 
* What about a L2 metric between color data? 


DONE: 
* Update tracking to use C4S3 and C3 rectangles. Probably need to restructure to track 
  one border at a time. 
* Add check for convexity of quadrangles: sometimes "boomerangs" get tracked
* Update tracking to follow quadrangles that have only three corners or sides visible
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
