use std::ffi::{CStr, CString};
use std::ptr;
use libsqlite3_sys::*;

fn main() {
    let db_path = CString::new("zed_test_db.sqlite").unwrap();
    let mut db: *mut sqlite3 = ptr::null_mut();

    let flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    
    let rc = unsafe { sqlite3_open_v2(db_path.as_ptr(), &mut db, flags, ptr::null()) };
    if rc != SQLITE_OK {
        let err_msg = unsafe { CStr::from_ptr(sqlite3_errmsg(db)).to_string_lossy() };
        panic!("Open failed (code {}): {}", rc, err_msg);
    }

    let pragmas = [
        "PRAGMA journal_mode = WAL;",
        "PRAGMA busy_timeout = 1;",
        "PRAGMA case_sensitive_like = TRUE;",
        "PRAGMA synchronous = NORMAL;",
        "PRAGMA foreign_keys = ON;"
    ];

    for pragma in pragmas {
        let sql = CString::new(pragma).unwrap();
        let rc = unsafe { sqlite3_exec(db, sql.as_ptr(), None, ptr::null_mut(), ptr::null_mut()) };
        if rc != SQLITE_OK {
            let err_msg = unsafe { CStr::from_ptr(sqlite3_errmsg(db)).to_string_lossy() };
            panic!("Pragma failed [{}] (code {}): {}", pragma, rc, err_msg);
        }
    }

    let table_sql = CString::new("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY);").unwrap();
    let rc = unsafe { sqlite3_exec(db, table_sql.as_ptr(), None, ptr::null_mut(), ptr::null_mut()) };
    if rc != SQLITE_OK {
        let err_msg = unsafe { CStr::from_ptr(sqlite3_errmsg(db)).to_string_lossy() };
        panic!("Migration failed (code {}): {}", rc, err_msg);
    }

    unsafe { sqlite3_close(db) };
    println!("SUCCESS");
}
