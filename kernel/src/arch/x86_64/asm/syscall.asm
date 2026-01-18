
extern sys_hello  
extern sys_futex_wake 
extern sys_futex_wait  
extern sys_openat  
extern sys_read 
extern sys_write 
extern sys_seek 
extern sys_close 
extern sys_tcb_set  
extern sys_libc_log  
extern sys_exit 
extern sys_mmap 
extern sys_free 
extern sys_stat 
extern sys_pipe 
extern sys_fork  
extern sys_dup  
extern sys_dup2 
extern sys_create_dev  
extern sys_iopl   
extern sys_ioctl 
extern sys_create_ioctl  
extern sys_setup_tty  
extern sys_isatty  
extern sys_setupmmap 
extern sys_access_framebuffer 
extern sys_ptsname 
extern sys_setup_ring_bytelen 
extern sys_read_dir
extern sys_exec
extern sys_getpid
extern sys_getppid
extern sys_gethostname 
extern sys_getcwd 
extern sys_waitpid  
extern sys_fcntl 
extern sys_fchdir 
extern sys_sleep 
extern sys_alloc_dma 
extern sys_map_phys 
extern sys_free_dma 
extern sys_connect 
extern sys_accept
extern sys_bind 
extern sys_socket 
extern sys_listen 
extern sys_timestamp 
extern sys_mkfifoat 
extern sys_poll  
extern sys_readlinkat 
extern sys_link 
extern sys_mkdirat 
extern sys_chmod 
extern sys_enabledebugmode 
extern sys_clone  
extern sys_hello 
extern sys_ttyname 
extern sys_breakpoint 
extern sys_copymemory 
extern sys_setpriority 
extern sys_getpriority  
extern sys_yield  
extern sys_rename  
extern sys_printdebuginfo  
extern sys_dmesg 
extern sys_enabledebugmodepid  
extern sys_socketpair  
extern sys_getuid 
extern sys_setuid 
extern sys_getsockname  
extern sys_getsockopt  
extern sys_msg_send 
extern sys_eventfd_create  
extern sys_msg_recv 
extern sys_kill  
extern sys_shutdown 
extern sys_shmget  
extern sys_shmat 
extern sys_shmdt 
extern sys_shmctl 
extern sys_getentropy 
extern sys_orangesigreturn
extern sys_orangedefaulthandler
extern sys_pause
extern sys_sigaction
extern sys_pwrite
extern sys_fstatfs
extern sys_fchmod
extern sys_fchmodat
extern sys_getaffinity
extern sys_cpucount

global syscall_handler
extern syscall_handler_c
syscall_handler:
    cli
    swapgs 
    mov qword [gs:0],rsp
    mov rsp, qword [gs:8]
    push qword (0x18 | 3)
    push qword [gs:0]
    push qword r11
    push qword (0x20 | 3)
    push qword rcx
    push qword 0
    push qword 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    mov rax,cr3
    push rax
    mov rdi,rsp
    call syscall_handler_c
    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    cli
    mov rsp, qword [gs:0] 
    swapgs
    o64 sysret

section .rodata

global syscall_table
syscall_table:
dq sys_hello  ; 0
dq sys_futex_wake ; 1 
dq sys_futex_wait  ; 2
dq sys_openat  ;3 
dq sys_read ; 4
dq sys_write ; 5
dq sys_seek ; 6
dq sys_close ; 7
dq sys_tcb_set  ; 8
dq sys_libc_log  ; 9
dq sys_exit ; 10
dq sys_mmap ; 11
dq sys_free  ;12
dq sys_stat  ;13
dq sys_pipe ;14
dq sys_fork  ;15
dq sys_dup  ;16
dq sys_dup2 ;17
dq sys_create_dev ;18  
dq sys_iopl   ;19
dq sys_ioctl ;20
dq sys_create_ioctl ;21  
dq sys_setup_tty  ;22
dq sys_isatty  ;23
dq sys_setupmmap ;24
dq sys_access_framebuffer ;25 
dq sys_ptsname ;26
dq sys_setup_ring_bytelen ;27 
dq sys_read_dir ; 28
dq sys_exec ; 29
dq sys_getpid ; 30
dq sys_getppid ; 31
dq sys_gethostname ;32
dq sys_getcwd ;33
dq sys_waitpid  ;34
dq sys_fcntl ;35
dq sys_fchdir ;36
dq sys_sleep ;37
dq sys_alloc_dma ;38 
dq sys_map_phys ;39
dq sys_free_dma ;40
dq sys_connect ;41
dq sys_accept
dq sys_bind ;42
dq sys_socket ;43
dq sys_listen ;44
dq sys_timestamp ;45
dq sys_mkfifoat ;46
dq sys_poll  ;47
dq sys_readlinkat ;48 
dq sys_link ;49
dq sys_mkdirat ;50
dq sys_chmod ;51
dq sys_enabledebugmode ;52 
dq sys_clone  ;53
dq sys_hello ;54
dq sys_ttyname ;55
dq sys_breakpoint ;56
dq sys_copymemory ;57
dq sys_setpriority ;58
dq sys_getpriority  ;59
dq sys_yield  ;60
dq sys_rename  ;61
dq sys_printdebuginfo ;62  
dq sys_dmesg ;63
dq sys_enabledebugmodepid ;64  
dq sys_socketpair  ;65
dq sys_getuid ;66
dq sys_setuid ;67
dq sys_getsockname ;68  
dq sys_getsockopt  ;69
dq sys_msg_send ;70
dq sys_eventfd_create ;71  
dq sys_msg_recv ;72 + 1
dq sys_kill  ;73 + 1
dq sys_shutdown ;74
dq sys_shmget  ;75
dq sys_shmat ;76
dq sys_shmdt ;77
dq sys_shmctl ;78
dq sys_getentropy ;80
dq sys_orangesigreturn ;81
dq sys_orangedefaulthandler ;82
dq sys_pause ;83
dq sys_sigaction ; 84
dq sys_pwrite ; 85
dq sys_fstatfs ;86
dq sys_fchmod ; 87
dq sys_fchmodat ;88
dq sys_getaffinity ;89
dq sys_cpucount ; 90