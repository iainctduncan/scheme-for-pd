# scheme-for-pd
Scheme for Pd (s4pd) is an open-source external for live-coding and scripting Pd 
with an embedded s7 Scheme Lisp interpreter. 

## Features in 0.1
* run code from files or Max messages
* load files using the Pd search path
* output numbers, symbols, lists
* basic array i/o
* send messages to named receivers
* schedule functions with **delay**

Scheme-for-Pd uses S7 Scheme, an embeddable Scheme implementation by Bill Schottstaedt at CCRMA. 
S7 is a minimal Scheme, with many nice features for algorithmic composition and embedding, 
and is the Scheme engine used in the Common Music algorithmic composition toolkit and the 
Snd audio editor. It has keywords, Common Lisp style macros, first-class environments, 
thread safety, applicative syntax, and a very straight forward FFI (foreign function interface).
Linguistically, it is mostly R4RS with some later extensions, along with some features 
from Common Lisp, and is similar in many ways to Guile, Clojure, and Janet.

## June 10, 2021 Status
* s4pd 0.1 is ready for testers who are able to build from source
* master branch should always build, and s4pd-help.pd should work
* log is chatty and the logging controls aren't finished
* compiler issues buckets of warnings that I need to clean up
* delaying events works, but clock clean up not yet implemented, meaning
  if you reset or recreate s4pd with outstanding clocks running, it may crash
* I really don't know Pd external packaging, but am putting up this version
  so early testers can help me get that right

## Building & Testing
* builds on my machine (OSX) with the build.sh script, which
  uses pd-lib-builder, but possibly incorrectly
* s4pd-help.pd is in the patchers dir, must be on file path I think
* Scheme sources in scm dir, Pd will need to find s4pd.scm 

Please let me know in GitHub issues or on the GitHub discussion page if
you find bugs, the help is unclear, or you have suggestions for building 
and packaging properly.

Please note that I am not taking pull requests at this time as this is a
thesis project, but you are encouraged to fork it if it's useful to you! 

Scheme for Pd was created by Iain C T Duncan.
s7 Scheme was created by Bill Schottstaedt.
As s7 is BSD licensed, so is Scheme for Pd. 
