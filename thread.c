/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "thread.h"
#include "limits.h"

#if _WIN32 && _WIN32_WINNT >= 0x0600
  bool sqfs_threads_available(void) {
    return true;
  }

  sqfs_err sqfs_mutex_init(sqfs_mutex *m) {
    InitializeCriticalSection(m);
    return SQFS_OK;
  }
  sqfs_err sqfs_mutex_destroy(sqfs_mutex *m) {
    DeleteCriticalSection(m);
    return SQFS_OK;
  }
  sqfs_err sqfs_mutex_lock(sqfs_mutex *m) {
    EnterCriticalSection(m);
    return SQFS_OK;
  }
  sqfs_err sqfs_mutex_unlock(sqfs_mutex *m) {
    LeaveCriticalSection(m);
    return SQFS_OK;
  }

  sqfs_err sqfs_cond_init(sqfs_cond_var *cv) {
    InitializeConditionVariable(cv);
    return SQFS_OK;
  }
  sqfs_err sqfs_cond_destroy(sqfs_cond_var *cv) {
    return SQFS_OK;
  }
  sqfs_err sqfs_cond_wait(sqfs_cond_var *cv, sqfs_mutex *m)  {
    return SleepConditionVariableCS(cv, m, INFINITE) ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_cond_signal(sqfs_cond_var *cv) {
    WakeConditionVariable(cv);
    return SQFS_OK;
  }
  sqfs_err sqfs_cond_broadcast(sqfs_cond_var *cv)  {
    WakeAllConditionVariable(cv);
    return SQFS_OK;
  }
#elif _WIN32
  bool sqfs_threads_available(void) {
    return true;
  }

  sqfs_err sqfs_mutex_init(sqfs_mutex *m) {
    *m = CreateMutex(NULL, FALSE, NULL);
    return *m ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_mutex_destroy(sqfs_mutex *m) {
    return CloseHandle(*m) ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_mutex_lock(sqfs_mutex *m) {
    return (WaitForSingleObject(*m, INFINITE) == WAIT_FAILED) ? SQFS_ERR
      : SQFS_OK;
  }
  sqfs_err sqfs_mutex_unlock(sqfs_mutex *m) {
    return ReleaseMutex(*m) ? SQFS_OK : SQFS_ERR;
  }

  sqfs_err sqfs_cond_init(sqfs_cond_var *cv) {
    InitializeCriticalSection(&cv->lock);
    EnterCriticalSection(&cv->lock);
    
    cv->waiters = 0;
    cv->sem = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    
    LeaveCriticalSection(&cv->lock);
    return cv->sem ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_cond_destroy(sqfs_cond_var *cv) {
    EnterCriticalSection(&cv->lock);
    BOOL closed = CloseHandle(cv->sem);
    LeaveCriticalSection(&cv->lock);
    DeleteCriticalSection(&cv->lock);
    return closed ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_cond_wait(sqfs_cond_var *cv, sqfs_mutex *m)  {
    EnterCriticalSection(&cv->lock);
    cv->waiters++;
    LeaveCriticalSection(&cv->lock);
    
    DWORD status = SignalObjectAndWait(*m, cv->sem, INFINITE, FALSE);
    
    EnterCriticalSection(&cv->lock);
    cv->waiters--;
    LeaveCriticalSection(&cv->lock);
    
    WaitForSingleObject(*m, INFINITE);
    return (status == WAIT_FAILED) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_cond_signal(sqfs_cond_var *cv) {
    EnterCriticalSection(&cv->lock);
    BOOL ok = ReleaseSemaphore(cv->sem, 1, NULL);
    LeaveCriticalSection(&cv->lock);
    return ok ? SQFS_OK : SQFS_ERR;
  }
  sqfs_err sqfs_cond_broadcast(sqfs_cond_var *cv)  {
    EnterCriticalSection(&cv->lock);
    BOOL ok = ReleaseSemaphore(cv->sem, cv->waiters, NULL);
    LeaveCriticalSection(&cv->lock);
    return ok ? SQFS_OK : SQFS_ERR;
  }
#elif HAVE_PTHREAD
  bool sqfs_threads_available(void) {
    return true;
  }
  
  sqfs_err sqfs_mutex_init(sqfs_mutex *m) {
    return pthread_mutex_init(m, NULL) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_mutex_destroy(sqfs_mutex *m) {
    return pthread_mutex_destroy(m) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_mutex_lock(sqfs_mutex *m) {
    return pthread_mutex_lock(m) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_mutex_unlock(sqfs_mutex *m) {
    return pthread_mutex_unlock(m) ? SQFS_ERR : SQFS_OK;
  }

  sqfs_err sqfs_cond_init(sqfs_cond_var *cv) {
    return pthread_cond_init(cv, NULL) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_cond_destroy(sqfs_cond_var *cv) {
    return pthread_cond_destroy(cv) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_cond_wait(sqfs_cond_var *cv, sqfs_mutex *m)  {
    return pthread_cond_wait(cv, m) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_cond_signal(sqfs_cond_var *cv) {
    return pthread_cond_signal(cv) ? SQFS_ERR : SQFS_OK;
  }
  sqfs_err sqfs_cond_broadcast(sqfs_cond_var *cv)  {
    return pthread_cond_broadcast(cv) ? SQFS_ERR : SQFS_OK;
  }
#else
  bool sqfs_threads_available(void) {
    return false;
  }
  
  sqfs_err sqfs_mutex_init(sqfs_mutex *SQFS_UNUSED(m)) { return SQFS_OK; }
  sqfs_err sqfs_mutex_destroy(sqfs_mutex *SQFS_UNUSED(m)) { return SQFS_OK; }
  sqfs_err sqfs_mutex_lock(sqfs_mutex *SQFS_UNUSED(m)) { return SQFS_OK; }
  sqfs_err sqfs_mutex_unlock(sqfs_mutex *SQFS_UNUSED(m)) { return SQFS_OK; }

  sqfs_err sqfs_cond_init(sqfs_cond_var *SQFS_UNUSED(cv)) { return SQFS_OK; }
  sqfs_err sqfs_cond_destroy(sqfs_cond_var *SQFS_UNUSED(cv))
    { return SQFS_OK; }
  sqfs_err sqfs_cond_wait(sqfs_cond_var *SQFS_UNUSED(cv),
    sqfs_mutex *SQFS_UNUSED(m)) { return SQFS_OK; }
  sqfs_err sqfs_cond_signal(sqfs_cond_var *SQFS_UNUSED(cv)) { return SQFS_OK; }
  sqfs_err sqfs_cond_broadcast(sqfs_cond_var *SQFS_UNUSED(cv))
    { return SQFS_OK; }
#endif
