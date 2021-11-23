;(post "schedule.scm")

; internal registry of callbacks registered by gensyms
(define s4pd-callback-registry (hash-table))

(define (s4pd-register-callback cb-function)
  (let ((key (gensym)))
    ;(post "registering" cb-function "with key" key)
    (set! (s4pd-callback-registry key) cb-function)
    key))

;; fetch a callback from the registry 
(define (s4pd-get-callback key)
  ;;(post "s4pd-getcallback, key:" key)
  (let ((cb-function (s4pd-callback-registry key)))
    cb-function))

;; internal function to get a callback from the registry and run it
;; this gets called from C code when the C timing function happens
;; todo: later add env support
(define (s4pd-execute-callback key)
  ;(post "s4pd-execute-callback" key)
  ;; get the func, note that this might return false if was cancelled
  (let ((cb-fun (s4pd-get-callback key)))
    ;; dereg the handle
    (set! (s4pd-callback-registry key) #f)
    ;; if callback retrieval got false, return false else execute function
    (if (eq? #f cb-fun) 
      '()
      ;; call our cb function, catching any errors here and posting
      (catch #t (lambda () (cb-fun)) (lambda err-args (post "ERROR:" err-args))))
))

; public function to delay a function by time ms (int or float)
; returns the gensym callback key, which can be used to cancel it
(define (delay time fun)
  ;(post "(delay) time:" time "fun:" fun)
  ;; register the callback and return the handle
  (let ((cb-handle (s4pd-register-callback fun)))
    ;; call the C ffi function and return the handle
    (s4pd-schedule-delay time cb-handle)
    cb-handle))


(define (cancel-delay key)
  "cancel a clock by erasing its scheme callback"
  ;(post "cancel-delay for key" key)
  (set! (s4pd-callback-registry key) #f)
  '())

(define (clear-delays)
  "cancel all scheduled delays"
  ;(post "(clear-delays)")
  (let ((delay-keys (map car s4pd-callback-registry)))
    ;(post "delay-keys to clear:" delay-keys)
    (for-each cancel-delay delay-keys)
    (s4pd-cancel-clocks)
    '()))

; flag to tell s4pd this loaded ok
(define s4pd-schedule-loaded #t)      

; return nil to avoid any confusing logging to console on boot
'()
;(post "s4pd-schedule.scm loaded")

