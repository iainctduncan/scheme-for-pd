; s4pd.scm - bootstrap file
; add to this file to create your bootstrap code
; removing functionality from this can can break s4pd, make a backup! 

;(post "s4pd.scm loading")

(load-from-path "s74.scm")
(load-from-path "s4pd-schedule.scm")

; eval function that runs in the root environment no matter where called
(define s4pd-eval 
  (lambda args (eval args (rootlet))))

;(post "s4pd.scm loaded")
