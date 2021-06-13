# scheme-for-pd
Scheme for Pd (s4pd) is an open-source external for live-coding and scripting Pd 
with an embedded s7 Scheme Lisp interpreter. It is a port of most of
Scheme for Max, by the same author, for Max/MSP.

## Features in 0.1
* run code from files or Pd messages
* load files using the Pd search path
* output numbers, symbols, lists
* basic array i/o
* send messages to named receivers
* schedule functions with **delay**

Scheme-for-Pd uses s7 Scheme, an embeddable Scheme implementation by Bill Schottstaedt at CCRMA. 
s7 is a minimal Scheme, with many nice features for algorithmic composition and embedding, 
and is the Scheme engine used in the Common Music algorithmic composition toolkit and the 
Snd audio editor. It has keywords, Common Lisp style macros, first-class environments, 
thread safety, applicative syntax, and a very straight forward FFI (foreign function interface).
Linguistically, it is mostly R4RS with some later extensions, along with some features 
from Common Lisp, and is similar in many ways to Guile, Clojure, and Janet.

## June 12, 2021 Status
* s4pd 0.1 is ready for testers who are able to build from source
* master branch should always build, and s4pd-help.pd should work
* no features of s4pd except endless loops should make it crash, let me
  know please if it does
* compiler issues buckets of warnings that I need to clean up (later)
* packaging for general releases is the next priority, advice welcome!

## Building & Testing
* builds on my machine (OSX) using pd-lib-builder and Make
* Scheme sources in scm dir, which must be on the Pd file path
* s4pd-help.pd is in the patchers dir, must be on file path I think

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
