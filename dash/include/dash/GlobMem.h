/* 
 * dash-lib/GlobMem.h
 *
 * author(s): Karl Fuerlinger, LMU Munich 
 */
/* @DASH_HEADER@ */

#ifndef DASH__GLOBMEM_H_
#define DASH__GLOBMEM_H_

#include <dash/dart/if/dart.h>
#include <dash/GlobPtr.h>
#include <dash/Team.h>

namespace dash {

enum class GlobMemKind {
  COLLECTIVE,
  LOCAL
};

constexpr GlobMemKind COLLECTIVE { GlobMemKind::COLLECTIVE };
constexpr GlobMemKind COLL       { GlobMemKind::COLLECTIVE };
constexpr GlobMemKind LOCAL      { GlobMemKind::LOCAL };

template<typename T>
void put_value(const T & newval, const GlobPtr<T> & gptr) {
  // BLOCKING !!
  dart_put_blocking(
    gptr.dartptr(),
    (void *)(&newval),
    sizeof(T)); 
}

template<typename T>
void get_value(T * ptr, const GlobPtr<T> & gptr) {
  // BLOCKING !!
  dart_get_blocking(
    ptr,
    gptr.dartptr(),
		sizeof(T));
}

template<typename TYPE>
class GlobMem {
private:
  dart_gptr_t  m_begptr;
  dart_team_t  m_teamid;
  size_t       m_nunits;
  size_t       m_nlelem;
  GlobMemKind  m_kind;
  TYPE       * m_lbegin;
  TYPE       * m_lend;

public:
  /**
   * Constructor, collectively allocates the given number of elements in
   * local memory of every unit in a team.
   */
  GlobMem(
    /// Team containing all units operating on global memory
    Team & team,
    /// Number of local elements to allocate
    size_t nlelem
  ) {
    m_begptr     = DART_GPTR_NULL;
    m_teamid     = team.dart_id();
    m_nlelem     = nlelem;
    m_kind       = COLLECTIVE;
    size_t lsize = sizeof(TYPE) * nlelem;
    dart_team_size(m_teamid, &m_nunits);
    dart_team_memalloc_aligned(
      m_teamid, 
			lsize,
      &m_begptr);
    m_lbegin     = lbegin(team.myid());
    m_lend       = lend(team.myid());
  }

  /**
   * Constructor, allocates the given number of elements in local memory.
   */
  GlobMem(
      /// [IN] Number of local elements to allocate
      size_t nlelem) {
    m_begptr     = DART_GPTR_NULL;
    m_teamid     = DART_TEAM_NULL;
    m_nlelem     = nlelem;
    m_nunits     = 1;
    m_kind       = LOCAL;
    size_t lsize = sizeof(TYPE) * nlelem;
    dart_memalloc(lsize, &m_begptr);
    m_lbegin     = lbegin(dash::myid());
    m_lend       = lend(dash::myid());
  }

  /**
   * Destructor, collectively frees underlying global memory.
   */
  ~GlobMem() {
    if (!DART_GPTR_ISNULL(m_begptr)) {
      if (m_kind == COLLECTIVE) {
        dart_team_memfree(m_teamid, m_begptr);
      } else {
        dart_memfree(m_begptr);
      } 
    }
  }

  /**
   * Global pointer of the initial address of the global memory.
   */
  const GlobPtr<TYPE> begin() const {
    return GlobPtr<TYPE>(m_begptr);
  }

  /**
   * Global pointer of the initial address of the global memory.
   */
  GlobPtr<TYPE> begin() {
    return GlobPtr<TYPE>(m_begptr);
  }

  /**
   * Native pointer of the initial address of the local memory of
   * a unit.
   */
  const TYPE * lbegin(dart_unit_t unit_id) const {
    void *addr;
    dart_gptr_t gptr = begin().dartptr();
    dart_gptr_setunit(&gptr, unit_id);
    dart_gptr_getaddr(gptr, &addr);
    return static_cast<const TYPE *>(addr);
  }

  /**
   * Native pointer of the initial address of the local memory of
   * a unit.
   */
  TYPE * lbegin(dart_unit_t unit_id) {
    void *addr;
    dart_gptr_t gptr = begin().dartptr();
    dart_gptr_setunit(&gptr, unit_id);
    dart_gptr_getaddr(gptr, &addr);
    return static_cast<TYPE *>(addr);
  }

  /**
   * Native pointer of the initial address of the local memory of
   * the unit that initialized this GlobMem instance.
   */
  const TYPE * lbegin() const {
    return m_lbegin;
  }

  /**
   * Native pointer of the initial address of the local memory of
   * the unit that initialized this GlobMem instance.
   */
  TYPE * lbegin() {
    return m_lbegin;
  }

  /**
   * Native pointer of the final address of the local memory of
   * a unit.
   */
  const TYPE * lend(dart_unit_t unit_id) const {
    // TODO: Why not lbegin() + m_lsize?
    void *addr;
    dart_gptr_t gptr = begin().dartptr();
    dart_gptr_setunit(&gptr, unit_id);
    dart_gptr_incaddr(&gptr, m_nlelem * sizeof(TYPE));
    dart_gptr_getaddr(gptr, &addr);
    return static_cast<const TYPE *>(addr);
  }

  /**
   * Native pointer of the final address of the local memory of
   * a unit.
   */
  TYPE * lend(dart_unit_t unit_id) {
    // TODO: Why not lbegin() + m_lsize?
    void *addr;
    dart_gptr_t gptr = begin().dartptr();
    dart_gptr_setunit(&gptr, unit_id);
    dart_gptr_incaddr(&gptr, m_nlelem * sizeof(TYPE));
    dart_gptr_getaddr(gptr, &addr);
    return static_cast<TYPE *>(addr);
  }

  /**
   * Native pointer of the initial address of the local memory of
   * the unit that initialized this GlobMem instance.
   */
  const TYPE * lend() const {
    return m_lend;
  }

  /**
   * Native pointer of the initial address of the local memory of
   * the unit that initialized this GlobMem instance.
   */
  TYPE * lend() {
    return m_lend;
  }

  template<typename T=TYPE>
  void put_value(const T & newval, size_t idx) {
    // idx to gptr
  }

  template<typename T=TYPE>
  void get_value(T * ptr, size_t idx) {
  }

  /**
   * Resolve the global pointer from an element position in a unit's
   * local memory.
   */
  GlobPtr<TYPE> index_to_gptr(
    /// The unit id
    size_t unit,
    /// The unit's local address offset
    long long idx) const {
    dart_unit_t lunit, gunit; 
    // TODO: Clarify: what is lunit, gunit?
    dart_gptr_t gptr = m_begptr;
    dart_team_unit_g2l(m_teamid, gptr.unitid, &lunit);
    lunit = (lunit + unit) % m_nunits;
    dart_team_unit_l2g(m_teamid, lunit, &gunit);
    
    dart_gptr_setunit(&gptr, gunit);
    dart_gptr_incaddr(&gptr, idx * sizeof(TYPE));

    return GlobPtr<TYPE>(gptr);
  }
#if 0
  /**
   * Resolve the global pointer from a unit's local pointer
   */
  GlobPtr<TYPE> ptr_to_gptr(
    /// The unit id
    dart_unit_t unit,
    /// The unit's local address
    const TYPE * local_ptr) const {
    GlobPtr<TYPE> gptr = begin();
    gptr.set_unit(unit);
    auto lptrdiff = local_ptr - begin();
    gptr += static_cast<long long>(lptrdiff);
    return gptr;
  }
#endif
};

template<typename T>
GlobPtr<T> memalloc(size_t nelem) {
  dart_gptr_t gptr;
  size_t lsize = sizeof(T) * nelem;
  
  dart_memalloc(lsize, &gptr);
  return GlobPtr<T>(gptr);
}

} // namespace dash

#endif // DASH__GLOBMEM_H_
