#include <memory>

template <typename T>
class copy_on_write
{
  std::shared_ptr<T> ptr_;

public:
  ~copy_on_write() = default;
  
  copy_on_write() = default;
  
  copy_on_write(T t) : ptr_(std::make_shared<T>(std::move(t)))
  {
  }

  copy_on_write(const copy_on_write& p) : ptr_(p.ptr_)
  {
  }
  
  copy_on_write(copy_on_write&& p) : ptr_(std::move(p.ptr_))
  {
  }

  copy_on_write& operator=(const copy_on_write& p)
  {
    ptr_ = p.ptr_;
    return *this;
  }
  
  copy_on_write& operator=(copy_on_write&& p)
  {
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
    return ptr_.operator->();
  }

  T& operator*()
  {
    assert(ptr_);
    if (!ptr_.unique())
    {
      ptr_ = std::make_shared<T>(*ptr_);
    }
    return *ptr_;
  }

  T* operator->()
  {
    assert(ptr_);
    if (!ptr_.unique())
    {
      ptr_ = std::make_shared<T>(*ptr_);
    }
    return ptr_.operator->();
  }
};
