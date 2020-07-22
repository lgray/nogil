#ifndef Py_OBJECT_H
#define Py_OBJECT_H

#include "pyatomic.h"

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define _PY_UNLIKELY(x)     __builtin_expect((x),0)
#define _PY_LIKELY(x)       __builtin_expect((x),1)
#else
#define _PY_UNLIKELY(x)     (x)
#define _PY_LIKELY(x)       (x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Object and type object interface */

/*
Objects are structures allocated on the heap.  Special rules apply to
the use of objects to ensure they are properly garbage-collected.
Objects are never allocated statically or on the stack; they must be
accessed through special macros and functions only.  (Type objects are
exceptions to the first rule; the standard types are represented by
statically initialized type objects, although work on type/class unification
for Python 2.2 made it possible to have heap-allocated type objects too).

An object has a 'reference count' that is increased or decreased when a
pointer to the object is copied or deleted; when the reference count
reaches zero there are no references to the object left and it can be
removed from the heap.

An object has a 'type' that determines what it represents and what kind
of data it contains.  An object's type is fixed when it is created.
Types themselves are represented as objects; an object contains a
pointer to the corresponding type object.  The type itself has a type
pointer pointing to the object representing the type 'type', which
contains a pointer to itself!).

Objects do not float around in memory; once allocated an object keeps
the same size and address.  Objects that must hold variable-size data
can contain pointers to variable-size parts of the object.  Not all
objects of the same type have the same size; but the size cannot change
after allocation.  (These restrictions are made so a reference to an
object can be simply a pointer -- moving an object would require
updating all the pointers, and changing an object's size would require
moving it if there was another object right next to it.)

Objects are always accessed through pointers of the type 'PyObject *'.
The type 'PyObject' is a structure that only contains the reference count
and the type pointer.  The actual memory allocated for an object
contains other data that can only be accessed after casting the pointer
to a pointer to a longer structure type.  This longer type must start
with the reference count and type fields; the macro PyObject_HEAD should be
used for this (to accommodate for future changes).  The implementation
of a particular object type can cast the object pointer to the proper
type and back.

A standard interface exists for objects that contain an array of items
whose size is determined when the object is allocated.
*/

/* Py_DEBUG implies Py_REF_DEBUG except in no GIL builds. */
#if defined(Py_DEBUG) && !defined(Py_REF_DEBUG)
#define Py_REF_DEBUG
#endif

#if defined(Py_LIMITED_API) && defined(Py_REF_DEBUG)
#error Py_LIMITED_API is incompatible with Py_DEBUG, Py_TRACE_REFS, and Py_REF_DEBUG
#endif

/* PyTypeObject structure is defined in cpython/object.h.
   In Py_LIMITED_API, PyTypeObject is an opaque structure. */
typedef struct _typeobject PyTypeObject;

#ifdef Py_TRACE_REFS
/* Define pointers to support a doubly-linked list of all live heap objects. */
#define _PyObject_HEAD_EXTRA            \
    struct _object *_ob_next;           \
    struct _object *_ob_prev;

#define _PyObject_EXTRA_INIT 0, 0,

#else
#define _PyObject_HEAD_EXTRA
#define _PyObject_EXTRA_INIT
#endif

/* PyObject_HEAD defines the initial segment of every PyObject. */
#define PyObject_HEAD                   PyObject ob_base;

#define _PyObject_STRUCT_INIT(type)     \
    { _PyObject_EXTRA_INIT              \
    0, 1, 0, type }

#define PyObject_HEAD_INIT(type)        \
    _PyObject_STRUCT_INIT(type),

#define PyVarObject_HEAD_INIT(type, size)       \
    { PyObject_HEAD_INIT(type) size },

/* PyObject_VAR_HEAD defines the initial segment of all variable-size
 * container objects.  These end with a declaration of an array with 1
 * element, but enough space is malloc'ed so that the array actually
 * has room for ob_size elements.  Note that ob_size is an element count,
 * not necessarily a byte count.
 */
#define PyObject_VAR_HEAD      PyVarObject ob_base;
#define Py_INVALID_SIZE (Py_ssize_t)-1

#define _PyObject_ThreadId(ob) (_Py_atomic_load_uintptr_relaxed(&_PyObject_CAST(ob)->ob_tid))

/* Nothing is actually declared to be a PyObject, but every pointer to
 * a Python object can be cast to a PyObject*.  This is inheritance built
 * by hand.  Similarly every pointer to a variable-size Python object can,
 * in addition, be cast to PyVarObject*.
 */
typedef struct _object {
    _PyObject_HEAD_EXTRA
    uintptr_t ob_tid;
    uint32_t ob_ref_local;
    volatile uint32_t ob_ref_shared;
    PyTypeObject *ob_type;
} PyObject;

/* Cast argument to PyObject* type. */
#define _PyObject_CAST(op) ((PyObject*)(op))

typedef struct {
    PyObject ob_base;
    Py_ssize_t ob_size; /* Number of items in variable part */
} PyVarObject;

/* Cast argument to PyVarObject* type. */
#define _PyVarObject_CAST(op) ((PyVarObject*)(op))

#define Py_REFCNT(ob)           (_PyObject_Refcount(_PyObject_CAST(ob)))
#define Py_IS_REFERENCED(ob)    (_PyObject_IsReferened(_PyObject_CAST(ob)))
#define Py_TYPE(ob)             (_PyObject_CAST(ob)->ob_type)
#define Py_SIZE(ob)             (_PyVarObject_CAST(ob)->ob_size)

static inline void _Py_SET_REFCNT(PyObject *ob, uint32_t refcnt) {
    ob->ob_ref_local = refcnt;
}
#define Py_SET_REFCNT(ob, refcnt) _Py_SET_REFCNT(_PyObject_CAST(ob), refcnt)

static inline void _Py_SET_TYPE(PyObject *ob, PyTypeObject *type) {
    ob->ob_type = type;
}
#define Py_SET_TYPE(ob, type) _Py_SET_TYPE(_PyObject_CAST(ob), type)

static inline void _Py_SET_SIZE(PyVarObject *ob, Py_ssize_t size) {
    ob->ob_size = size;
}
#define Py_SET_SIZE(ob, size) _Py_SET_SIZE(_PyVarObject_CAST(ob), size)


/*
Type objects contain a string containing the type name (to help somewhat
in debugging), the allocation parameters (see PyObject_New() and
PyObject_NewVar()),
and methods for accessing objects of the type.  Methods are optional, a
nil pointer meaning that particular kind of access is not available for
this type.  The Py_DECREF() macro uses the tp_dealloc method without
checking for a nil pointer; it should always be implemented except if
the implementation can guarantee that the reference count will never
reach zero (e.g., for statically allocated type objects).

NB: the methods for certain type groups are now contained in separate
method blocks.
*/

typedef PyObject * (*unaryfunc)(PyObject *);
typedef PyObject * (*binaryfunc)(PyObject *, PyObject *);
typedef PyObject * (*ternaryfunc)(PyObject *, PyObject *, PyObject *);
typedef int (*inquiry)(PyObject *);
typedef Py_ssize_t (*lenfunc)(PyObject *);
typedef PyObject *(*ssizeargfunc)(PyObject *, Py_ssize_t);
typedef PyObject *(*ssizessizeargfunc)(PyObject *, Py_ssize_t, Py_ssize_t);
typedef int(*ssizeobjargproc)(PyObject *, Py_ssize_t, PyObject *);
typedef int(*ssizessizeobjargproc)(PyObject *, Py_ssize_t, Py_ssize_t, PyObject *);
typedef int(*objobjargproc)(PyObject *, PyObject *, PyObject *);

typedef int (*objobjproc)(PyObject *, PyObject *);
typedef int (*visitproc)(PyObject *, void *);
typedef int (*traverseproc)(PyObject *, visitproc, void *);


typedef void (*freefunc)(void *);
typedef void (*destructor)(PyObject *);
typedef PyObject *(*getattrfunc)(PyObject *, char *);
typedef PyObject *(*getattrofunc)(PyObject *, PyObject *);
typedef int (*setattrfunc)(PyObject *, char *, PyObject *);
typedef int (*setattrofunc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*reprfunc)(PyObject *);
typedef Py_hash_t (*hashfunc)(PyObject *);
typedef PyObject *(*richcmpfunc) (PyObject *, PyObject *, int);
typedef PyObject *(*getiterfunc) (PyObject *);
typedef PyObject *(*iternextfunc) (PyObject *);
typedef PyObject *(*descrgetfunc) (PyObject *, PyObject *, PyObject *);
typedef int (*descrsetfunc) (PyObject *, PyObject *, PyObject *);
typedef int (*initproc)(PyObject *, PyObject *, PyObject *);
typedef PyObject *(*newfunc)(PyTypeObject *, PyObject *, PyObject *);
typedef PyObject *(*allocfunc)(PyTypeObject *, Py_ssize_t);

typedef struct{
    int slot;    /* slot id, see below */
    void *pfunc; /* function pointer */
} PyType_Slot;

typedef struct{
    const char* name;
    int basicsize;
    int itemsize;
    unsigned int flags;
    PyType_Slot *slots; /* terminated by slot==0. */
} PyType_Spec;

PyAPI_FUNC(PyObject*) PyType_FromSpec(PyType_Spec*);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_FUNC(PyObject*) PyType_FromSpecWithBases(PyType_Spec*, PyObject*);
#endif
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03040000
PyAPI_FUNC(void*) PyType_GetSlot(PyTypeObject*, int);
#endif

/* Generic type check */
PyAPI_FUNC(int) PyType_IsSubtype(PyTypeObject *, PyTypeObject *);
#define PyObject_TypeCheck(ob, tp) \
    (Py_TYPE(ob) == (tp) || PyType_IsSubtype(Py_TYPE(ob), (tp)))

PyAPI_DATA(PyTypeObject) PyType_Type; /* built-in 'type' */
PyAPI_DATA(PyTypeObject) PyBaseObject_Type; /* built-in 'object' */
PyAPI_DATA(PyTypeObject) PySuper_Type; /* built-in 'super' */

PyAPI_FUNC(unsigned long) PyType_GetFlags(PyTypeObject*);

PyAPI_FUNC(int) PyType_Ready(PyTypeObject *);
PyAPI_FUNC(PyObject *) PyType_GenericAlloc(PyTypeObject *, Py_ssize_t);
PyAPI_FUNC(PyObject *) PyType_GenericNew(PyTypeObject *,
                                               PyObject *, PyObject *);
PyAPI_FUNC(unsigned int) PyType_ClearCache(void);
PyAPI_FUNC(void) PyType_Modified(PyTypeObject *);

/* Generic operations on objects */
PyAPI_FUNC(PyObject *) PyObject_Repr(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_Str(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_ASCII(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_Bytes(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_RichCompare(PyObject *, PyObject *, int);
PyAPI_FUNC(int) PyObject_RichCompareBool(PyObject *, PyObject *, int);
PyAPI_FUNC(PyObject *) PyObject_GetAttrString(PyObject *, const char *);
PyAPI_FUNC(int) PyObject_SetAttrString(PyObject *, const char *, PyObject *);
PyAPI_FUNC(int) PyObject_HasAttrString(PyObject *, const char *);
PyAPI_FUNC(PyObject *) PyObject_GetAttr(PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_SetAttr(PyObject *, PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_HasAttr(PyObject *, PyObject *);
PyAPI_FUNC(PyObject *) PyObject_SelfIter(PyObject *);
PyAPI_FUNC(PyObject *) PyObject_GenericGetAttr(PyObject *, PyObject *);
PyAPI_FUNC(int) PyObject_GenericSetAttr(PyObject *, PyObject *, PyObject *);
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03030000
PyAPI_FUNC(int) PyObject_GenericSetDict(PyObject *, PyObject *, void *);
#endif
PyAPI_FUNC(Py_hash_t) PyObject_Hash(PyObject *);
PyAPI_FUNC(Py_hash_t) PyObject_HashNotImplemented(PyObject *);
PyAPI_FUNC(int) PyObject_IsTrue(PyObject *);
PyAPI_FUNC(int) PyObject_Not(PyObject *);
PyAPI_FUNC(int) PyCallable_Check(PyObject *);
PyAPI_FUNC(void) PyObject_ClearWeakRefs(PyObject *);
PyAPI_FUNC(void) _PyObject_ClearWeakRefsFromGC(PyObject *);

/* PyObject_Dir(obj) acts like Python builtins.dir(obj), returning a
   list of strings.  PyObject_Dir(NULL) is like builtins.dir(),
   returning the names of the current locals.  In this case, if there are
   no current locals, NULL is returned, and PyErr_Occurred() is false.
*/
PyAPI_FUNC(PyObject *) PyObject_Dir(PyObject *);


/* Helpers for printing recursive container types */
PyAPI_FUNC(int) Py_ReprEnter(PyObject *);
PyAPI_FUNC(void) Py_ReprLeave(PyObject *);

/* Flag bits for printing: */
#define Py_PRINT_RAW    1       /* No string quotes etc. */


/*
Type flags (tp_flags)

These flags are used to change expected features and behavior for a
particular type.

Arbitration of the flag bit positions will need to be coordinated among
all extension writers who publicly release their extensions (this will
be fewer than you might expect!).

Most flags were removed as of Python 3.0 to make room for new flags.  (Some
flags are not for backwards compatibility but to indicate the presence of an
optional feature; these flags remain of course.)

Type definitions should use Py_TPFLAGS_DEFAULT for their tp_flags value.

Code can use PyType_HasFeature(type_ob, flag_value) to test whether the
given type object has a specified feature.
*/

/* Set if the type object is dynamically allocated */
#define Py_TPFLAGS_HEAPTYPE (1UL << 9)

/* Set if the type allows subclassing */
#define Py_TPFLAGS_BASETYPE (1UL << 10)

/* Set if the type implements the vectorcall protocol (PEP 590) */
#ifndef Py_LIMITED_API
#define Py_TPFLAGS_HAVE_VECTORCALL (1UL << 11)
// Backwards compatibility alias for API that was provisional in Python 3.8
#define _Py_TPFLAGS_HAVE_VECTORCALL Py_TPFLAGS_HAVE_VECTORCALL
#endif

/* Set if the type is 'ready' -- fully initialized */
#define Py_TPFLAGS_READY (1UL << 12)

/* Set while the type is being 'readied', to prevent recursive ready calls */
#define Py_TPFLAGS_READYING (1UL << 13)

/* Objects support garbage collection (see objimpl.h) */
#define Py_TPFLAGS_HAVE_GC (1UL << 14)

/* These two bits are preserved for Stackless Python, next after this is 17 */
#ifdef STACKLESS
#define Py_TPFLAGS_HAVE_STACKLESS_EXTENSION (3UL << 15)
#else
#define Py_TPFLAGS_HAVE_STACKLESS_EXTENSION 0
#endif

/* Objects behave like an unbound method */
#define Py_TPFLAGS_METHOD_DESCRIPTOR (1UL << 17)

/* Objects support type attribute cache */
#define Py_TPFLAGS_HAVE_VERSION_TAG   (1UL << 18)
#define Py_TPFLAGS_VALID_VERSION_TAG  (1UL << 19)

/* Type is abstract and cannot be instantiated */
#define Py_TPFLAGS_IS_ABSTRACT (1UL << 20)

/* These flags are used to determine if a type is a subclass. */
#define Py_TPFLAGS_LONG_SUBCLASS        (1UL << 24)
#define Py_TPFLAGS_LIST_SUBCLASS        (1UL << 25)
#define Py_TPFLAGS_TUPLE_SUBCLASS       (1UL << 26)
#define Py_TPFLAGS_BYTES_SUBCLASS       (1UL << 27)
#define Py_TPFLAGS_UNICODE_SUBCLASS     (1UL << 28)
#define Py_TPFLAGS_DICT_SUBCLASS        (1UL << 29)
#define Py_TPFLAGS_BASE_EXC_SUBCLASS    (1UL << 30)
#define Py_TPFLAGS_TYPE_SUBCLASS        (1UL << 31)

#define Py_TPFLAGS_DEFAULT  ( \
                 Py_TPFLAGS_HAVE_STACKLESS_EXTENSION | \
                 Py_TPFLAGS_HAVE_VERSION_TAG | \
                0)

/* NOTE: The following flags reuse lower bits (removed as part of the
 * Python 3.0 transition). */

/* The following flag is kept for compatibility. Starting with 3.8,
 * binary compatibility of C extensions across feature releases of
 * Python is not supported anymore, except when using the stable ABI.
 */

/* Type structure has tp_finalize member (3.4) */
#define Py_TPFLAGS_HAVE_FINALIZE (1UL << 0)


/*
The macros Py_INCREF(op) and Py_DECREF(op) are used to increment or decrement
reference counts.  Py_DECREF calls the object's deallocator function when
the refcount falls to 0; for
objects that don't contain references to other objects or heap memory
this can be the standard function free().  Both macros can be used
wherever a void expression is allowed.  The argument must not be a
NULL pointer.  If it may be NULL, use Py_XINCREF/Py_XDECREF instead.
The macro _Py_NewReference(op) initialize reference counts to 1, and
in special builds (Py_REF_DEBUG, Py_TRACE_REFS) performs additional
bookkeeping appropriate to the special build.

We assume that the reference count field can never overflow; this can
be proven when the size of the field is the same as the pointer size, so
we ignore the possibility.  Provided a C int is at least 32 bits (which
is implicitly assumed in many parts of this code), that's enough for
about 2**31 references to an object.

XXX The following became out of date in Python 2.2, but I'm not sure
XXX what the full truth is now.  Certainly, heap-allocated type objects
XXX can and should be deallocated.
Type objects should never be deallocated; the type pointer in an object
is not considered to be a reference to the type object, to save
complications in the deallocation function.  (This is actually a
decision that's up to the implementer of each new type so if you want,
you can count such references to the type object.)
*/

#ifdef Py_REF_DEBUG
PyAPI_FUNC(void) _Py_IncRefTotal(void);
PyAPI_FUNC(void) _Py_DecRefTotal(void);
PyAPI_FUNC(void) _Py_NegativeRefcount(const char *filename, int lineno,
                                      PyObject *op);
#endif /* Py_REF_DEBUG */


PyAPI_FUNC(void) _Py_Dealloc(PyObject *);
PyAPI_FUNC(void) _Py_NO_INLINE _Py_IncRefShared(PyObject *);
PyAPI_FUNC(void) _Py_NO_INLINE _Py_DecRefShared(PyObject *);
PyAPI_FUNC(int) _Py_TryIncRefShared(PyObject *);
PyAPI_FUNC(void) _Py_MergeZeroRefcount(PyObject *);
Py_ssize_t _Py_ExplicitMergeRefcount(PyObject *);

static inline uintptr_t
_Py_ThreadId(void)
{
#if defined(__GNUC__)
    // TODO: only AMD64 fix i386
    uintptr_t out;
    #if defined(__MACH__)
    __asm__ ("mov %%gs:0, %0" : "=r" (out));  // value of fs
    #else
    __asm__ ("mov %%fs:0, %0" : "=r" (out));  // value of fs
    #endif
    return out;
#elif defined(_WIN64) && defined(_X86_)
    // TODO: figure out what happens when you target 32-bit Windows from 64-bit windows
    return __readgsdword(0x48);  // current thread ID
#elif defined(_WIN32) && defined(_X86_)
    return __readfsdword(0x24);  // current thread ID
#else
    return 0;
#endif
}

static inline int
_Py_ThreadLocal(PyObject *op)
{
    uintptr_t *ob_tid = &op->ob_tid;
#if defined(__GNUC__) && defined(__GCC_ASM_FLAG_OUTPUTS__)
    uintptr_t tmp;
    int out;
    __asm__ ("mov    %%fs:0, %[tmp]\n\t"
             "cmp    %[tmp], %[ob_tid]"
        : [tmp] "=&r"(tmp), "=@ccz" (out)
        : [ob_tid] "m" (*ob_tid)
        : );
    return out;
#else
    return _Py_atomic_load_uintptr_relaxed(ob_tid) == _Py_ThreadId();
#endif
}

#define _Py_REF_LOCAL_SHIFT     2
#define _Py_REF_IMMORTAL_MASK   0x1
#define _Py_REF_DEFERRED_MASK   0x2

#define _Py_REF_SHARED_SHIFT    2
#define _Py_REF_QUEUED_MASK     0x2
#define _Py_REF_MERGED_MASK     0x1

static inline int
_Py_REF_IS_IMMORTAL(uint32_t local)
{
    return (local & _Py_REF_IMMORTAL_MASK) != 0;
}

static inline int
_PyObject_IS_IMMORTAL(PyObject *op)
{
    return _Py_REF_IS_IMMORTAL(op->ob_ref_local);
}

static inline int
_PyObject_IS_DEFERRED_RC(PyObject *op)
{
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    return (local & _Py_REF_DEFERRED_MASK) != 0;
}

static inline int
_Py_REF_IS_MERGED(uint32_t shared)
{
    return (shared & _Py_REF_MERGED_MASK) != 0;
}

static inline int
_Py_REF_IS_QUEUED(uint32_t shared)
{
    return (shared & _Py_REF_QUEUED_MASK) != 0;
}

_Py_ALWAYS_INLINE static inline void
_Py_INCREF(PyObject *op)
{
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    if (_Py_REF_IS_IMMORTAL(local)) {
        return;
    }

#ifdef Py_REF_DEBUG
    _Py_IncRefTotal();
#endif
    if (_PY_LIKELY(_Py_ThreadLocal(op))) {
        local += (1 << _Py_REF_LOCAL_SHIFT);
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);
    }
    else {
        _Py_IncRefShared(op);
    }
}

#define Py_INCREF(op) _Py_INCREF(_PyObject_CAST(op))

_Py_ALWAYS_INLINE static inline void
_Py_INCREF_STACK(PyObject *op)
{
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    if ((local & 0x3) != 0) {
        return;
    }

#ifdef Py_REF_DEBUG
    _Py_IncRefTotal();
#endif
    if (_PY_LIKELY(_Py_ThreadLocal(op))) {
        local += (1 << _Py_REF_LOCAL_SHIFT);
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);
    }
    else {
        _Py_IncRefShared(op);
    }
}

#define Py_INCREF_STACK(op) _Py_INCREF_STACK(_PyObject_CAST(op))

static inline int
_Py_TryIncref(PyObject *op) {
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    if (_Py_REF_IS_IMMORTAL(local)) {
        return 1;
    }

#ifdef Py_REF_DEBUG
    _Py_IncRefTotal();
#endif
    if (_PY_LIKELY(_Py_ThreadLocal(op))) {
        local += (1 << _Py_REF_LOCAL_SHIFT);
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);
        return 1;
    }
    else {
        return _Py_TryIncRefShared(op);
    }
}

#define _Py_TRY_INCREF(op) _Py_TryIncref(_PyObject_CAST(op))

_Py_ALWAYS_INLINE static inline void _Py_DECREF(
#ifdef Py_REF_DEBUG
    const char *filename, int lineno,
#endif
    PyObject *op)
{
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    if (_PY_UNLIKELY(_Py_REF_IS_IMMORTAL(local))) {
        return;
    }

#ifdef Py_REF_DEBUG
    _Py_DecRefTotal();
#endif
    if (_PY_LIKELY(_Py_ThreadLocal(op))) {
#ifdef Py_REF_DEBUG
        if (local == 0) {
            _Py_NegativeRefcount(filename, lineno, op);
        }
#endif
        local -= (1 << _Py_REF_LOCAL_SHIFT);
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);

        if (_PY_UNLIKELY(local < 4)) {
            _Py_MergeZeroRefcount(op);
        }
    }
    else {
        _Py_DecRefShared(op);
    }
}

#ifdef Py_REF_DEBUG
#  define Py_DECREF(op) _Py_DECREF(__FILE__, __LINE__, _PyObject_CAST(op))
#else
#  define Py_DECREF(op) _Py_DECREF(_PyObject_CAST(op))
#endif

_Py_ALWAYS_INLINE static inline void _Py_DECREF_STACK(
#ifdef Py_REF_DEBUG
    const char *filename, int lineno,
#endif
    PyObject *op)
{
    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    if ((local & (_Py_REF_IMMORTAL_MASK | _Py_REF_DEFERRED_MASK)) != 0) {
        return;
    }

#ifdef Py_REF_DEBUG
    _Py_DecRefTotal();
#endif
    if (_PY_LIKELY(_Py_ThreadLocal(op))) {
#ifdef Py_REF_DEBUG
        if (local == 0) {
            _Py_NegativeRefcount(filename, lineno, op);
        }
#endif
        local -= (1 << _Py_REF_LOCAL_SHIFT);
        _Py_atomic_store_uint32_relaxed(&op->ob_ref_local, local);

        if (_PY_UNLIKELY(local < 4)) {
            _Py_MergeZeroRefcount(op);
        }
    }
    else {
        _Py_DecRefShared(op);
    }
}

#ifdef Py_REF_DEBUG
#  define Py_DECREF_STACK(op) _Py_DECREF_STACK(__FILE__, __LINE__, _PyObject_CAST(op))
#else
#  define Py_DECREF_STACK(op) _Py_DECREF_STACK(_PyObject_CAST(op))
#endif


/* Safely decref `op` and set `op` to NULL, especially useful in tp_clear
 * and tp_dealloc implementations.
 *
 * Note that "the obvious" code can be deadly:
 *
 *     Py_XDECREF(op);
 *     op = NULL;
 *
 * Typically, `op` is something like self->containee, and `self` is done
 * using its `containee` member.  In the code sequence above, suppose
 * `containee` is non-NULL with a refcount of 1.  Its refcount falls to
 * 0 on the first line, which can trigger an arbitrary amount of code,
 * possibly including finalizers (like __del__ methods or weakref callbacks)
 * coded in Python, which in turn can release the GIL and allow other threads
 * to run, etc.  Such code may even invoke methods of `self` again, or cause
 * cyclic gc to trigger, but-- oops! --self->containee still points to the
 * object being torn down, and it may be in an insane state while being torn
 * down.  This has in fact been a rich historic source of miserable (rare &
 * hard-to-diagnose) segfaulting (and other) bugs.
 *
 * The safe way is:
 *
 *      Py_CLEAR(op);
 *
 * That arranges to set `op` to NULL _before_ decref'ing, so that any code
 * triggered as a side-effect of `op` getting torn down no longer believes
 * `op` points to a valid object.
 *
 * There are cases where it's safe to use the naive code, but they're brittle.
 * For example, if `op` points to a Python integer, you know that destroying
 * one of those can't cause problems -- but in part that relies on that
 * Python integers aren't currently weakly referenceable.  Best practice is
 * to use Py_CLEAR() even if you can't think of a reason for why you need to.
 */
#define Py_CLEAR(op)                            \
    do {                                        \
        PyObject *_py_tmp = _PyObject_CAST(op); \
        if (_py_tmp != NULL) {                  \
            (op) = NULL;                        \
            Py_DECREF(_py_tmp);                 \
        }                                       \
    } while (0)

/* Function to use in case the object pointer can be NULL: */
static inline void _Py_XINCREF(PyObject *op)
{
    if (op != NULL) {
        Py_INCREF(op);
    }
}

#define Py_XINCREF(op) _Py_XINCREF(_PyObject_CAST(op))

static inline void _Py_XDECREF(PyObject *op)
{
    if (op != NULL) {
        Py_DECREF(op);
    }
}

#define Py_XDECREF(op) _Py_XDECREF(_PyObject_CAST(op))

static inline void
_PyRef_UnpackLocal(uint32_t bits, Py_ssize_t *refcount, int *immortal)
{
    *refcount = bits >> _Py_REF_LOCAL_SHIFT;
    *immortal = (bits & _Py_REF_IMMORTAL_MASK) != 0;
}

static inline void
_PyRef_UnpackShared(uint32_t bits, Py_ssize_t *refcount, int *queued, int *merged)
{
    *refcount = Py_ARITHMETIC_RIGHT_SHIFT(int32_t,
                                          (int32_t)bits,
                                          _Py_REF_SHARED_SHIFT);
    if (queued) {
        *queued = (bits & _Py_REF_QUEUED_MASK) != 0;
    }
    if (merged) {
        *merged = (bits & _Py_REF_MERGED_MASK) != 0;
    }
}

static inline int
_PyObject_HasLocalRefcnt(PyObject *op, Py_ssize_t v)
{
    Py_ssize_t local_refcount, shared_refcount;
    int immortal;

    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    _PyRef_UnpackLocal(local, &local_refcount, &immortal);

    if (immortal) {
        return 0;
    }
    if (_PyObject_ThreadId(op) != _Py_ThreadId()) {
        return 0;
    }
    if (local_refcount != v) {
        return 0;
    }

    uint32_t shared = _Py_atomic_load_uint32_relaxed(&op->ob_ref_shared);
    _PyRef_UnpackShared(shared, &shared_refcount, NULL, NULL);

    return shared_refcount == 0;
}

static inline Py_ssize_t
_PyObject_Refcount(PyObject *op)
{
    Py_ssize_t local_refcount, shared_refcount;
    int immortal;

    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    _PyRef_UnpackLocal(local, &local_refcount, &immortal);

    if (immortal) {
        return 999;
    }

    uint32_t shared = _Py_atomic_load_uint32_relaxed(&op->ob_ref_shared);
    _PyRef_UnpackShared(shared, &shared_refcount, NULL, NULL);

    return local_refcount + shared_refcount;
}

static inline int
_PyObject_IsReferened(PyObject *op)
{
    Py_ssize_t local_refcount, shared_refcount;
    int immortal;

    uint32_t local = _Py_atomic_load_uint32_relaxed(&op->ob_ref_local);
    _PyRef_UnpackLocal(local, &local_refcount, &immortal);

    if (immortal) {
        return 1;
    }
    else if (_PyObject_ThreadId(op) == _Py_ThreadId()) {
        return 1;
    }

    uint32_t shared = _Py_atomic_load_uint32_relaxed(&op->ob_ref_shared);
    _PyRef_UnpackShared(shared, &shared_refcount, NULL, NULL);

    return (local_refcount + shared_refcount) > 0;
}

/*
These are provided as conveniences to Python runtime embedders, so that
they can have object code that is not dependent on Python compilation flags.
*/
PyAPI_FUNC(void) Py_IncRef(PyObject *);
PyAPI_FUNC(void) Py_DecRef(PyObject *);
PyAPI_FUNC(Py_ssize_t) _Py_RefCnt(PyObject *);

/*
_Py_NoneStruct is an object of undefined type which can be used in contexts
where NULL (nil) is not suitable (since NULL often means 'error').

Don't forget to apply Py_INCREF() when returning this value!!!
*/
PyAPI_DATA(PyObject) _Py_NoneStruct; /* Don't use this directly */
#define Py_None (&_Py_NoneStruct)

/* Macro for returning Py_None from a function */
#define Py_RETURN_NONE return Py_None

/*
Py_NotImplemented is a singleton used to signal that an operation is
not implemented for a given type combination.
*/
PyAPI_DATA(PyObject) _Py_NotImplementedStruct; /* Don't use this directly */
#define Py_NotImplemented (&_Py_NotImplementedStruct)

/* Macro for returning Py_NotImplemented from a function */
#define Py_RETURN_NOTIMPLEMENTED return Py_NotImplemented

/* Rich comparison opcodes */
#define Py_LT 0
#define Py_LE 1
#define Py_EQ 2
#define Py_NE 3
#define Py_GT 4
#define Py_GE 5

/*
 * Macro for implementing rich comparisons
 *
 * Needs to be a macro because any C-comparable type can be used.
 */
#define Py_RETURN_RICHCOMPARE(val1, val2, op)                               \
    do {                                                                    \
        switch (op) {                                                       \
        case Py_EQ: if ((val1) == (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_NE: if ((val1) != (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_LT: if ((val1) < (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;   \
        case Py_GT: if ((val1) > (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;   \
        case Py_LE: if ((val1) <= (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        case Py_GE: if ((val1) >= (val2)) Py_RETURN_TRUE; Py_RETURN_FALSE;  \
        default:                                                            \
            Py_UNREACHABLE();                                               \
        }                                                                   \
    } while (0)


/*
More conventions
================

Argument Checking
-----------------

Functions that take objects as arguments normally don't check for nil
arguments, but they do check the type of the argument, and return an
error if the function doesn't apply to the type.

Failure Modes
-------------

Functions may fail for a variety of reasons, including running out of
memory.  This is communicated to the caller in two ways: an error string
is set (see errors.h), and the function result differs: functions that
normally return a pointer return NULL for failure, functions returning
an integer return -1 (which could be a legal return value too!), and
other functions return 0 for success and -1 for failure.
Callers should always check for errors before using the result.  If
an error was set, the caller must either explicitly clear it, or pass
the error on to its caller.

Reference Counts
----------------

It takes a while to get used to the proper usage of reference counts.

Functions that create an object set the reference count to 1; such new
objects must be stored somewhere or destroyed again with Py_DECREF().
Some functions that 'store' objects, such as PyTuple_SetItem() and
PyList_SetItem(),
don't increment the reference count of the object, since the most
frequent use is to store a fresh object.  Functions that 'retrieve'
objects, such as PyTuple_GetItem() and PyDict_GetItemString(), also
don't increment
the reference count, since most frequently the object is only looked at
quickly.  Thus, to retrieve an object and store it again, the caller
must call Py_INCREF() explicitly.

NOTE: functions that 'consume' a reference count, like
PyList_SetItem(), consume the reference even if the object wasn't
successfully stored, to simplify error handling.

It seems attractive to make other functions that take an object as
argument consume a reference count; however, this may quickly get
confusing (even the current practice is already confusing).  Consider
it carefully, it may save lots of calls to Py_INCREF() and Py_DECREF() at
times.
*/

#ifndef Py_LIMITED_API
#  define Py_CPYTHON_OBJECT_H
#  include  "cpython/object.h"
#  undef Py_CPYTHON_OBJECT_H
#endif


static inline int
PyType_HasFeature(PyTypeObject *type, unsigned long feature) {
#ifdef Py_LIMITED_API
    return ((PyType_GetFlags(type) & feature) != 0);
#else
    return ((type->tp_flags & feature) != 0);
#endif
}

#define PyType_FastSubclass(type, flag) PyType_HasFeature(type, flag)

static inline int _PyType_Check(PyObject *op) {
    return PyType_FastSubclass(Py_TYPE(op), Py_TPFLAGS_TYPE_SUBCLASS);
}
#define PyType_Check(op) _PyType_Check(_PyObject_CAST(op))

static inline int _PyType_CheckExact(PyObject *op) {
    return (Py_TYPE(op) == &PyType_Type);
}
#define PyType_CheckExact(op) _PyType_CheckExact(_PyObject_CAST(op))

#ifdef __cplusplus
}
#endif
#endif /* !Py_OBJECT_H */
