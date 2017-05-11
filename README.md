# die-toy
A GUI that allows for annotation and manipulation of chip die images.

Dependencies: <br />
Qt5 <br />
OpenCv v3.1.x+ <br />
Cmake 2.8.8+ <br />

Build instructions: <br />
> mkdir build <br />
> cd build <br />
> make <br />

Operating instructions:
> dieToy --help <br />
&nbsp;&nbsp;-v, --version                    Displays version information. <br />
&nbsp;&nbsp;-i, --image <filename>           Die image to load. <br />
&nbsp;&nbsp;-d, --dieDescription <filename>  Die description file to load. <br />

* Load an image. <br />
  (Mousewheel zooms, middle mouse button drags) <br />
* Switch into Bounds Define mode. <br />
* Click 4 points to define the bounds of the ROM region <br />
* Switch into horizontal / vertical slice mode & define some strips where bits appear <br />
* Switch into bit region display mode and export bit PNG or do various other fun things.
