;(post "schedule.scm")

; internal registry of callbacks registered by gensyms
(define s4pd-callback-registry (hash-table))
; and for pd C clocks (as void ponters)
(define s4pd-clock-registry (hash-table))

(define (s4pd-register-callback cb-function)
  (let ((key (gensym)))
    ;(post "registering" cb-function "with key" key)
    (set! (s4pd-callback-registry key) cb-function)
    key))

(define (s4pd-register-clock handle clock-ptr)
  ;(post "registering pd clock:" handle clock-ptr)
  (set! (s4pd-clock-registry handle) clock-ptr))

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
  (post "(delay) time:" time "fun:" fun)
  ;; register the callback and return the handle
  (let ((cb-handle (s4pd-register-callback fun)))
    ;; call the C ffi function and return the handle
    (s4pd-schedule-delay time cb-handle)
    cb-handle))


(define (cancel-delay key)
  ;;(post "de-registering callback for key" key)
  (set! (s4pd-callback-registry key) #f))


;; delay with either a list or var args, 
;; i.e. (delay 1000 (list out 0 'bang)) or (delay-eval 1000 post :foo :bar)
;(define (delay-eval time . args)
;  ;;(post "delay-eval* time: " time "args: " list-arg)
;  (let ((cb-fun (if (list? (car args)) 
;                  (lambda x (eval (car args))) 
;                  (lambda x (eval args)))))
;    (delay time cb-fun)))    


(post "s4pd-schedule.scm loaded")

