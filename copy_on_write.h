#include <memory>

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
struct control_block
{
  virtual ~control_block() = default;
  virtual std::unique_ptr<control_block> clone() const = 0;
  virtual T* ptr() = 0;
};

template <typename T, typename U, typename C = default_copy<U>,
          typename D = default_delete<U>>
class pointer_control_block : public control_block<T>
{
  std::unique_ptr<U, D> p_;
  C c_;

public:
  explicit pointer_control_block(U* u, C c = C{}, D d = D{})
      : c_(std::move(c)), p_(u, std::move(d))
  {
  }

  std::unique_ptr<control_block<T>> clone() const override
  {
    assert(p_);
    return std::make_unique<pointer_control_block>(c_(*p_), c_,
                                                   p_.get_deleter());
  }

  T* ptr() override
  {
    return p_.get();
  }
};

template <typename T, typename U = T>
class direct_control_block : public control_block<U>
{
  U u_;

public:
  template <typename... Ts>
  explicit direct_control_block(Ts&&... ts) : u_(U(std::forward<Ts>(ts)...))
  {
  }

  std::unique_ptr<control_block<U>> clone() const override
  {
    return std::make_unique<direct_control_block>(*this);
  }

  T* ptr() override
  {
    return &u_;
  }
};

template <typename T, typename U>
class delegating_control_block : public control_block<T>
{

  std::unique_ptr<control_block<U>> delegate_;

public:
  explicit delegating_control_block(std::unique_ptr<control_block<U>> b)
      : delegate_(std::move(b))
  {
  }

  std::unique_ptr<control_block<T>> clone() const override
  {
    return std::make_unique<delegating_control_block>(delegate_->clone());
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
  std::shared_ptr<control_block<T>> cb_;

  void detach()
  {
    auto p = cb_->clone();
    cb_ = std::shared_ptr<control_block<T>>(p.release());
    ptr_ = cb_->ptr();
  }

public:
  ~copy_on_write() = default;

  copy_on_write() = default;

  copy_on_write(T t) : ptr_(std::make_shared<T>(std::move(t)))
  {
  }

  copy_on_write(const copy_on_write& p) : ptr_(p.ptr_), cb_(p.cb_)
  {
  }

  copy_on_write(copy_on_write&& p) : ptr_(std::move(p.ptr_)), cb_(std::move(p.cb_))
  {
  }

  copy_on_write& operator=(const copy_on_write& p)
  {
    cb_ = p.cb_;
    ptr_ = p.ptr_;
    return *this;
  }

  copy_on_write& operator=(copy_on_write&& p)
  {
    cb_ = std::move(p.cb_);
    ptr_ = std::move(p.ptr_);
    return *this;
  }

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

  T& operator*()
  {
    assert(ptr_);
    if (!cb_.shared())
    {
      detach();
    }
    return *ptr_;
  }

  T* operator->()
  {
    assert(ptr_);
    if (!cb_.shared())
    {
      detach();
    }
    return ptr_;
  }
};
