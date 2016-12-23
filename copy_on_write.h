#include <memory>
#include <type_traits>
#include <cassert>

////////////////////////////////////////////////////////////////////////////////
// Implementation detail classes
////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct default_copy
{
  T* operator()(const T& t) const
  {
    return new T(t);
  }
};

template <typename T>
struct default_delete
{
  void operator()(const T* t) const
  {
    delete t;
  }
};

template <typename T>
struct shared_control_block
{
  virtual ~shared_control_block() = default;
  virtual std::shared_ptr<shared_control_block> clone() const = 0;
  virtual T* ptr() = 0;
};

template <typename T, typename U, typename C = default_copy<U>,
          typename D = default_delete<U>>
class indirect_shared_control_block : public shared_control_block<T>
{
  std::unique_ptr<U, D> p_;
  C c_;

public:
  explicit indirect_shared_control_block(U* u, C c = C{}, D d = D{})
      : c_(std::move(c)), p_(u, std::move(d))
  {
  }

  std::shared_ptr<shared_control_block<T>> clone() const override
  {
    assert(p_);
    return std::make_shared<indirect_shared_control_block>(c_(*p_), c_,
                                                   p_.get_deleter());
  }

  T* ptr() override
  {
    return p_.get();
  }
};

template <typename T, typename U = T>
class direct_shared_control_block : public shared_control_block<U>
{
  U u_;

public:
  template <typename... Ts>
  explicit direct_shared_control_block(Ts&&... ts) : u_(U(std::forward<Ts>(ts)...))
  {
  }

  std::shared_ptr<shared_control_block<U>> clone() const override
  {
    return std::make_shared<direct_shared_control_block>(*this);
  }

  T* ptr() override
  {
    return &u_;
  }
};

template <typename T, typename U>
class delegating_shared_control_block : public shared_control_block<T>
{

  std::shared_ptr<shared_control_block<U>> delegate_;

public:
  explicit delegating_shared_control_block(std::shared_ptr<shared_control_block<U>> b)
      : delegate_(b)
  {
  }

  std::shared_ptr<shared_control_block<T>> clone() const override
  {
    return std::make_shared<delegating_shared_control_block>(delegate_->clone());
  }

  T* ptr() override
  {
    return delegate_->ptr();
  }
};

template <typename T>
class copy_on_write;

template <typename T>
struct is_copy_on_write : std::false_type
{
};

template <typename T>
struct is_copy_on_write<copy_on_write<T>> : std::true_type
{
};

////////////////////////////////////////////////////////////////////////////////
// `copy_on_write` class definition
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class copy_on_write
{
  template <typename U>
  friend class copy_on_write;
  template <typename T_, typename... Ts>
  friend copy_on_write<T_> make_copy_on_write(Ts&&... ts);

  T* ptr_ = nullptr;
  std::shared_ptr<shared_control_block<T>> cb_;

  void detach()
  {
    cb_ = cb_->clone();
    ptr_ = cb_->ptr();
  }

public:

  //
  // Destructor
  //

  ~copy_on_write() = default;

  //
  // Constructors
  //

  copy_on_write()
  {
  }

  template <typename U, typename C = default_copy<U>,
            typename D = default_delete<U>,
            typename V = std::enable_if_t<std::is_convertible<U*, T*>::value>>
  explicit copy_on_write(U* u, C copier = C{}, D deleter = D{})
  {
    if (!u)
    {
      return;
    }

    assert(typeid(*u) == typeid(U));

    cb_ = std::make_unique<indirect_shared_control_block<T, U, C, D>>(
        u, std::move(copier), std::move(deleter));
    ptr_ = u;
  }

  template<typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value && !is_copy_on_write<U>::value>>
  copy_on_write(U u) : copy_on_write(new U(std::move(u)))
  {
  }

  //
  // Copy constructors
  //

  copy_on_write(const copy_on_write& c) : ptr_(c.ptr_), cb_(c.cb_)
  {
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_convertible<U*, T*>::value>>
  copy_on_write(const copy_on_write<U>& p)
  {
    copy_on_write<U> tmp(p);
    ptr_ = tmp.ptr_;
    cb_ = std::static_pointer_cast<shared_control_block<T>>(
        std::make_shared<delegating_shared_control_block<T, U>>(
            std::move(tmp.cb_)));
  }

  //
  // Move constructors
  //

  copy_on_write(copy_on_write&& c) : ptr_(std::move(c.ptr_)), cb_(std::move(c.cb_))
  {
    c.ptr_ = nullptr;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_convertible<U*, T*>::value>>
  copy_on_write(copy_on_write<U>&& c)
  {
    ptr_ = c.ptr_;
    cb_ = std::static_pointer_cast<shared_control_block<T>>(
        std::make_shared<delegating_shared_control_block<T, U>>(
            std::move(c.cb_)));
    c.ptr_ = nullptr;
  }

  //
  // Copy assignment
  //

  copy_on_write& operator=(const copy_on_write& p)
  {
    if (&p == this)
    {
      return *this;
    }

    if (!p)
    {
      cb_.reset();
      ptr_ = nullptr;
      return *this;
    }

    auto tmp_cb = p.cb_->clone();
    ptr_ = tmp_cb->ptr();
    cb_ = std::move(tmp_cb);
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_convertible<U*, T*>::value>>
  copy_on_write& operator=(const copy_on_write<U>& p)
  {
    copy_on_write<U> tmp(p);
    *this = std::move(tmp);
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<std::is_convertible<U*, T*>::value &&
                                          !is_copy_on_write<U>::value>>
  copy_on_write& operator=(const U& u)
  {
    copy_on_write tmp(u);
    *this = std::move(tmp);
    return *this;
  }


  //
  // Move assignment
  //

  copy_on_write& operator=(copy_on_write&& p) noexcept
  {
    if (&p == this)
    {
      return *this;
    }

    cb_ = std::move(p.cb_);
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<!std::is_same<T, U>::value &&
                                          std::is_convertible<U*, T*>::value>>
  copy_on_write& operator=(copy_on_write<U>&& p)
  {
    cb_ = std::make_unique<delegating_shared_control_block<T, U>>(std::move(p.cb_));
    ptr_ = p.ptr_;
    p.ptr_ = nullptr;
    return *this;
  }

  template <typename U,
            typename V = std::enable_if_t<std::is_convertible<U*, T*>::value &&
                                          !is_copy_on_write<U>::value>>
  copy_on_write& operator=(U&& u)
  {
    copy_on_write tmp(std::move(u));
    *this = std::move(tmp);
    return *this;
  }

  //
  // Modifiers
  //

  void swap(copy_on_write& c) noexcept
  {
    using std::swap;
    swap(ptr_, c.ptr_);
    swap(cb_, c.cb_);
  }


  //
  // Observers
  //

  explicit operator bool() const
  {
    return bool(ptr_);
  }

  const T& operator*() const
  {
    assert(ptr_);
    return *ptr_;
  }

  const T* operator->() const
  {
    assert(ptr_);
    return ptr_;
  }
  
  const T& value() const
  {
    return *ptr_;
  }

  //
  // Mutator
  //

  friend T* mutate(copy_on_write& c)
  {
    if ( c.ptr_ && ! c.cb_.unique() )
    {
      c.detach();
    }
    return c.ptr_;
  }

  //
  // non-member swap
  //

  friend void swap(copy_on_write& t, copy_on_write& u) noexcept
  {
    t.swap(u);
  }
};

//
// copy_on_write creation
//

template <typename T, typename... Ts>
copy_on_write<T> make_copy_on_write(Ts&&... ts)
{
  copy_on_write<T> p;
  p.cb_ = std::make_unique<direct_shared_control_block<T>>(std::forward<Ts>(ts)...);
  p.ptr_ = p.cb_->ptr();
  return std::move(p);
}

