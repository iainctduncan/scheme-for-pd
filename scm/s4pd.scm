; s4pd.scm - bootstrap file
; add to this file to create your bootstrap code
; removing functionality from this can can break s4pd, make a backup! 
;(post "s4pd.scm loading")

(load-from-path "s74.scm")
(load-from-path "s4pd-schedule.scm")

; eval function that runs in the root environment no matter where called
(define s4pd-eval 
  (lambda args (eval args (rootlet))))


; the listeners registry is a nested hash-table of inlet number and listen keyword/symbol 
(define s4pd-listeners (make-hash-table))

;; the listen function, used to register listeners for inlet > 0
(define (listen inlet key fun)
  ;(post "adding listener on inlet " inlet " with key " key) 
  ; create nested hash at key {inlet-num} if not there already
  (if (not (s4pd-listeners inlet)) 
    (set! (s4pd-listeners inlet) (make-hash-table)))
  (set! ((s4pd-listeners inlet) key) fun))

;; dispatch that matches S4M's
;; dispatch is called from C, and is passed a list of:
;; ({inlet} {key} .. args...)
(define (s4pd-dispatch inlet key . args)
  ;(post "s4pd-dispatch, inlet:" inlet "key:" key)
  ;(post "s4pd-listeners:" s4pd-listeners)
  (let ((listener (if (s4pd-listeners inlet) ((s4pd-listeners inlet) key) #f)))
    ;(post "found listener:" listener)
    (if (procedure? listener) 
       (apply listener args)
       (post "Error: no listener on inlet" inlet "for" key))))

; to register a function with the s4pd dispatch function above,
; (listen 1 'foo foo-fun)


;*******************************************************************************
; Alternatively, you can also make your own s4pd-dispatcher
; - inlet will be inlet number (always 1 at the moment)
; - args will be a list of all the remaining args as symbols or floats

; this dispatcher looks for a function with the name of the first
; symbol and then calls this with the remaining args
; unlike inlet 0, symbols sent to s4pd this way become Scheme symbols
; rather than variable references.
; 
;(define (s4pd-dispatch inlet . args)
;  (let* ((fun-sym (car args))
;         (fun (eval fun-sym))
;         (fun-args (cdr args)))
;    (if (procedure? fun)
;      (apply fun fun-args)
;      (post "no function defined for message" fun-sym))))

; flag to tell s4pd this loaded ok
(define s4pd-loaded #t)

; return nil to avoid any confusing logging to console on boot
'()
