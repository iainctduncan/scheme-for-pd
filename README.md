# scheme-for-pd
Scheme for Pd (s4pd) is an open-source external for live-coding and scripting Pd 
with an embedded s7 Scheme Lisp interpreter. It is a port of most of
Scheme for Max by the same author, and enables most Scheme code to be run on either.
As of 0.1 beta, there are binaries up for Mac and Windows. It will be published to Deken once I know Windows users are having success with the binaries.

## Features in 0.1
* Run code from files, and hot reload files
* Evaluate Scheme code from Pd messages with a REPL 
* Output numbers, symbols, lists
* Basic array i/o
* Send messages to named receivers
* Schedule functions with delay

Scheme-for-Pd uses s7 Scheme, an embeddable Scheme implementation by Bill Schottstaedt at CCRMA. 
s7 is a minimal Scheme, with many nice features for algorithmic composition and embedding, 
and is the Scheme engine used in the Common Music algorithmic composition toolkit and the 
Snd audio editor. It has keywords, Common Lisp style macros, first-class environments, 
thread safety, applicative syntax, and a very straight forward FFI (foreign function interface).
Linguistically, it is mostly R4RS with some later extensions, along with some features 
from Common Lisp, and is similar in many ways to Guile, Clojure, and Janet.

## Installation 
* s4pd is available as source you can build, and as a beta release download for Mac (no M1) and Windows (32 and 64) https://github.com/iainctduncan/scheme-for-pd/releases/tag/0.1-beta
* Expand the zip file contents in your Pd/externals directory. This includes all the scm files and the help patcher, so you should only need this directory to be on your Pd file search path (in File Preferences)
* If you build from source, see notes below. 
* If s4pd can't find the scm files, you will get warnings about this in the console. 
* You may need to add "[declare -path s4pd]" to your patcher, or to edit your file preferences to add the path
  where the external and the various scm files live.
* Video walk throughs of the installation process are up on the YouTube "Music With Lisp" channel: https://www.youtube.com/channel/UC6ftX7yuEi5uUFkRVJbJyWA

## Building from Source
* Builds on OSX, Windows, and Linux using pd-lib-builder.
* Scheme sources live in the scm sub directory but get copied to the s4pd directory on make install

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

s7 Scheme was created by Bill Schottstaedt, based originally on Tiny Scheme.

Scheme for Pd and s7 are open source under the BSD-2 license. 
You may use these in whatever kind of project you wish!
