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

  std::unique_ptr<shared_control_block<U>> delegate_;

public:
  explicit delegating_shared_control_block(std::unique_ptr<shared_control_block<U>> b)
      : delegate_(std::move(b))
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
  T* ptr_ = nullptr;
  std::shared_ptr<shared_control_block<T>> cb_;

  void detach()
  {
    auto p = cb_->clone();
    cb_ = std::shared_ptr<shared_control_block<T>>(p.release());
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
  
  template<typename U, typename = std::enable_if_t<std::is_base_of<T,U>::value && !is_copy_on_write<U>::value>>
  copy_on_write(U u) : copy_on_write(new U(std::move(u)))
  {
  }

  //
  // Copy constructors
  //
  
  copy_on_write(const copy_on_write& c) : ptr_(c.ptr_), cb_(c.cb_)
  {
  }

  //
  // Move constructors
  //
  
  copy_on_write(copy_on_write&& c) : ptr_(std::move(c.ptr_)), cb_(std::move(c.cb_))
  {
    c.ptr_ = nullptr;
  }
  
  //
  // Copy assignment
  //
  
  copy_on_write& operator=(const copy_on_write& c)
  {
    cb_ = c.cb_;
    ptr_ = c.ptr_;
    return *this;
  }

  //
  // Move assignment
  //
 
  copy_on_write& operator=(copy_on_write&& c)
  {
    cb_ = std::move(c.cb_);
    ptr_ = std::move(c.ptr_);
    c.ptr_ = nullptr;
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

  //
  // Mutator
  //

  friend T* mutate(copy_on_write& c)
  {
    return c.ptr_;
  }
};

