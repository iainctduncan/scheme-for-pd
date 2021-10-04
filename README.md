# scheme-for-pd
Scheme for Pd (s4pd) is an open-source external for live-coding and scripting Pd 
with an embedded s7 Scheme Lisp interpreter. It is a port of most of
Scheme for Max, by the same author, for Max/MSP.

## Features in 0.1
* run code from files, and hot reload files
* evaluate scheme code from Pd messages with a REPL 
* output numbers, symbols, lists
* basic array i/o
* send messages to named receivers
* schedule functions with delay

Scheme-for-Pd uses s7 Scheme, an embeddable Scheme implementation by Bill Schottstaedt at CCRMA. 
s7 is a minimal Scheme, with many nice features for algorithmic composition and embedding, 
and is the Scheme engine used in the Common Music algorithmic composition toolkit and the 
Snd audio editor. It has keywords, Common Lisp style macros, first-class environments, 
thread safety, applicative syntax, and a very straight forward FFI (foreign function interface).
Linguistically, it is mostly R4RS with some later extensions, along with some features 
from Common Lisp, and is similar in many ways to Guile, Clojure, and Janet.

## Installation 
* s4pd is available as source you can build, and as a beta release download for OSX only (windows to come)
* If you install the release package, you should expand the tarball contents in your Pd/externals directory
* If you build from source, you must ensure the scm files are on a path that Pd searches. You can add the scm directory to your Pd path preferences, or copy the scm files to a folder in externals along with the s4pd external object (so that they are in the same directory)
* If s4pd can't find the scm files, you will get warnings about this in the console. Just fix this as above and you should be good.

## Building from Source
* Builds on my machine (OSX) using pd-lib-builder and Make
* Please let me know if you build successfully on Windows or Linux and I will update this section.
* Scheme sources in scm dir: this dir must be on the Pd file path.
* s4pd-help.pd is in the patchers dir, must be on file path I think
* If there is a windows dev who can help with windows releases, please get in touch

## Documentation
* Docs for s4pd do not yet exist, but the help file shows all features
* Much of the documentation for Scheme for Max applies 
  https://iainctduncan.github.io/scheme-for-max-docs/
* For people new to Scheme or Lisp, the  "Learn Scheme for Max" ebook is here
  https://iainctduncan.github.io/learn-scheme-for-max/

Please let me know in GitHub issues or on the GitHub discussion page if
you find bugs, the help is unclear, or you have suggestions for building 
and packaging properly.

Please note that I cannot take undiscussed pull requests at this time as this is a
thesis project, but forking is most welcome if it's useful to you.
I am happy to discuss improvements and take contributions if they make sense. 


## Credits and License
Scheme for Pd was created by Iain C T Duncan, 2021

s7 Scheme was created by Bill Schottstaedt.

Scheme for Pd and s7 are open source under the BSD-2 license.
