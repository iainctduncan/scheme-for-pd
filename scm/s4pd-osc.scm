(post "s4pd-osc.scm")

; override s4pd's default dispatcher with an osc handler
; that looks for functions named the same as the first osc message symbol
; you can see the default dispatcher (that we are replacing) in s4pd.scm
; note that for now, inlet will always just be 1
(define (s4pd-dispatch inlet . args)
  (let* ((fun-sym (car args))
         (fun (eval fun-sym))
         (fun-args (cdr args)))
    (if (procedure? fun)
      (apply fun fun-args)
      (post "no function defined for message" fun-sym))))


(define (button1 . args)
  (post "button1 running, args:" args))

(define (handler . args)
  (post "handler" args))




