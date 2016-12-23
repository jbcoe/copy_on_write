#define CATCH_CONFIG_MAIN

#include "copy_on_write.h"
#include <catch.hpp>

struct BaseType
{
  virtual int value() const = 0;
  virtual void set_value(int) = 0;
  virtual ~BaseType() = default;
};

struct DerivedType : BaseType
{
  int value_ = 0;

  DerivedType()
  {
    ++object_count;
  }

  DerivedType(const DerivedType& d)
  {
    value_ = d.value_;
    ++object_count;
  }

  DerivedType(int v) : value_(v)
  {
    ++object_count;
  }

  ~DerivedType()
  {
    --object_count;
  }

  int value() const override { return value_; }

  void set_value(int i) override { value_ = i; }

  static size_t object_count;
};

size_t DerivedType::object_count = 0;

TEST_CASE("Default constructor","[copy_on_write.constructors]")
{
  GIVEN("A default constructed copy_on_write to BaseType")
  {
    copy_on_write<BaseType> cptr;

    THEN("operator bool returns false")
    {
      REQUIRE((bool)cptr == false);
    }
  }

  GIVEN("A default constructed const copy_on_write to BaseType")
  {
    const copy_on_write<BaseType> ccptr;

    THEN("operator bool returns false")
    {
      REQUIRE((bool)ccptr == false);
    }
  }
}

TEST_CASE("Value constructor", "[copy_on_write.constructors]")
{
  DerivedType d(7);

  copy_on_write<BaseType> i(d);

  REQUIRE(i->value() == 7);
}

TEST_CASE("Value move-constructor", "[copy_on_write.constructors]")
{
  DerivedType d(7);

  copy_on_write<BaseType> i(d);

  REQUIRE(i->value() == 7);
}

TEST_CASE("Value assignment", "[copy_on_write.constructors]")
{
  DerivedType d(7);

  copy_on_write<BaseType> i;
  i = d;

  REQUIRE(i->value() == 7);
}

TEST_CASE("Value move-assignment", "[copy_on_write.constructors]")
{
  DerivedType d(7);

  copy_on_write<BaseType> i;
  i = std::move(d);

  REQUIRE(i->value() == 7);
}

TEST_CASE("Pointer constructor","[copy_on_write.constructors]")
{
  GIVEN("A pointer-constructed copy_on_write")
  {
    int v = 7;
    copy_on_write<BaseType> cptr(new DerivedType(v));

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(cptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)cptr == true);
    }
  }
  GIVEN("A pointer-constructed const copy_on_write")
  {
    int v = 7;
    const copy_on_write<BaseType> ccptr(new DerivedType(v));

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(ccptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)ccptr == true);
    }
  }
}

struct BaseCloneSelf
{
  BaseCloneSelf() = default;
  virtual ~BaseCloneSelf() = default;
  BaseCloneSelf(const BaseCloneSelf &) = delete;
  virtual std::unique_ptr<BaseCloneSelf> clone() const = 0;
};

struct DerivedCloneSelf : BaseCloneSelf
{
  static size_t object_count;
  std::unique_ptr<BaseCloneSelf> clone() const { return std::make_unique<DerivedCloneSelf>(); }
  DerivedCloneSelf() { ++object_count; }
  ~DerivedCloneSelf(){ --object_count; }
};

size_t DerivedCloneSelf::object_count = 0;

struct invoke_clone_member
{
  template <typename T> T *operator()(const T &t) const {
    return static_cast<T *>(t.clone().release());
  }
};

TEST_CASE("copy_on_write constructed with copier and deleter",
          "[copy_on_write.constructor]") {
  size_t copy_count = 0;
  size_t deletion_count = 0;
  auto cp = copy_on_write<DerivedType>(new DerivedType(),
                                    [&](const DerivedType &d) {
                                      ++copy_count;
                                      return new DerivedType(d);
                                    },
                                    [&](const DerivedType *d) {
                                      ++deletion_count;
                                      delete d;
                                    });
  {
    auto cp2 = cp;
    REQUIRE(copy_count == 1);
  }
  REQUIRE(deletion_count == 1);
}

TEST_CASE("copy_on_write destructor","[copy_on_write.destructor]")
{
  GIVEN("No derived objects")
  {
    REQUIRE(DerivedType::object_count == 0);

    THEN("Object count is increased on construction and decreased on destruction")
    {
      // begin and end scope to force destruction
      {
        copy_on_write<BaseType> tmp(new DerivedType());
        REQUIRE(DerivedType::object_count == 1);
      }
      REQUIRE(DerivedType::object_count == 0);
    }
  }
}

TEST_CASE("copy_on_write copy constructor","[copy_on_write.constructors]")
{
  GIVEN("A copy_on_write copied from a default-constructed copy_on_write")
  {
    copy_on_write<BaseType> original_cptr;
    copy_on_write<BaseType> cptr(original_cptr);

    THEN("operator bool returns false")
    {
      REQUIRE((bool)cptr == false);
    }
  }

  GIVEN("A copy_on_write copied from a pointer-constructed copy_on_write")
  {
    REQUIRE(DerivedType::object_count == 0);

    int v = 7;
    copy_on_write<BaseType> original_cptr(new DerivedType(v));
    copy_on_write<BaseType> cptr(original_cptr);

    THEN("values are distinct")
    {
      REQUIRE(&cptr.value() != &original_cptr.value());
    }

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(cptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)cptr == true);
    }

    THEN("object count is two")
    {
      REQUIRE(DerivedType::object_count == 2);
    }

    WHEN("Changes are made to the original copy_on_write after copying")
    {
      int new_value = 99;
      mutate(original_cptr)->set_value(new_value);
      REQUIRE(original_cptr->value() == new_value);
      THEN("They are not reflected in the copy (copy is distinct)")
      {
        REQUIRE(cptr->value() != new_value);
        REQUIRE(cptr->value() == v);
      }
    }
  }
}

TEST_CASE("copy_on_write move constructor","[copy_on_write.constructors]")
{
  GIVEN("A copy_on_write move-constructed from a default-constructed copy_on_write")
  {
    copy_on_write<BaseType> original_cptr;
    copy_on_write<BaseType> cptr(std::move(original_cptr));

    THEN("The original copy_on_write is empty")
    {
      REQUIRE(!(bool)original_cptr);
    }

    THEN("The move-constructed copy_on_write is empty")
    {
      REQUIRE(!(bool)cptr);
    }
  }

  GIVEN("A copy_on_write move-constructed from a default-constructed copy_on_write")
  {
    int v = 7;
    copy_on_write<BaseType> original_cptr(new DerivedType(v));
    auto original_pointer = &original_cptr.value();
    CHECK(DerivedType::object_count == 1);

    copy_on_write<BaseType> cptr(std::move(original_cptr));
    CHECK(DerivedType::object_count == 1);

    THEN("The original copy_on_write is empty")
    {
      REQUIRE(!(bool)original_cptr);
    }

    THEN("The move-constructed pointer is the original pointer")
    {
      REQUIRE(&cptr.value()==original_pointer);
      REQUIRE(cptr.operator->()==original_pointer);
      REQUIRE((bool)cptr);
    }

    THEN("The move-constructed pointer value is the constructed value")
    {
      REQUIRE(cptr->value() == v);
    }
  }
}

TEST_CASE("copy_on_write assignment","[copy_on_write.assignment]")
{
  GIVEN("A default-constructed copy_on_write assigned-to a default-constructed copy_on_write")
  {
    copy_on_write<BaseType> cptr1;
    const copy_on_write<BaseType> cptr2;
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 0);

    cptr1 = cptr2;

    REQUIRE(DerivedType::object_count == 0);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(&cptr2.value() == p);
    }

    THEN("The assigned-to object is empty")
    {
      REQUIRE(!cptr1);
    }
  }

  GIVEN("A default-constructed copy_on_write assigned to a pointer-constructed copy_on_write")
  {
    int v1 = 7;

    copy_on_write<BaseType> cptr1(new DerivedType(v1));
    const copy_on_write<BaseType> cptr2;
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 1);

    cptr1 = cptr2;

    REQUIRE(DerivedType::object_count == 0);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(&cptr2.value() == p);
    }

    THEN("The assigned-to object is empty")
    {
      REQUIRE(!cptr1);
    }
  }

  GIVEN("A pointer-constructed copy_on_write assigned to a default-constructed copy_on_write")
  {
    int v1 = 7;

    copy_on_write<BaseType> cptr1;
    const copy_on_write<BaseType> cptr2(new DerivedType(v1));
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 1);

    cptr1 = cptr2;

    REQUIRE(DerivedType::object_count == 2);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(&cptr2.value() == p);
    }

    THEN("The assigned-to object is non-empty")
    {
      REQUIRE((bool)cptr1);
    }

    THEN("The assigned-from object 'value' is the assigned-to object value")
    {
      REQUIRE(cptr1->value() == cptr2->value());
    }

    THEN("The assigned-from object pointer and the assigned-to object pointer are distinct")
    {
      REQUIRE(&cptr1.value() != &cptr2.value());
    }

  }

  GIVEN("A pointer-constructed copy_on_write assigned to a pointer-constructed copy_on_write")
  {
    int v1 = 7;
    int v2 = 87;

    copy_on_write<BaseType> cptr1(new DerivedType(v1));
    const copy_on_write<BaseType> cptr2(new DerivedType(v2));
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 2);

    cptr1 = cptr2;

    REQUIRE(DerivedType::object_count == 2);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(&cptr2.value() == p);
    }

    THEN("The assigned-to object is non-empty")
    {
      REQUIRE((bool)cptr1);
    }

    THEN("The assigned-from object 'value' is the assigned-to object value")
    {
      REQUIRE(cptr1->value() == cptr2->value());
    }

    THEN("The assigned-from object pointer and the assigned-to object pointer are distinct")
    {
      REQUIRE(&cptr1.value() != &cptr2.value());
    }
  }

  GIVEN("A pointer-constructed copy_on_write assigned to itself")
  {
    int v1 = 7;

    copy_on_write<BaseType> cptr1(new DerivedType(v1));
    const auto p = &cptr1.value();

    REQUIRE(DerivedType::object_count == 1);

    cptr1 = cptr1;

    REQUIRE(DerivedType::object_count == 1);

    THEN("The assigned-from object is unchanged")
    {
      REQUIRE(&cptr1.value() == p);
    }
  }
}

TEST_CASE("copy_on_write move-assignment","[copy_on_write.assignment]")
{
  GIVEN("A default-constructed copy_on_write move-assigned-to a default-constructed copy_on_write")
  {
    copy_on_write<BaseType> cptr1;
    copy_on_write<BaseType> cptr2;
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 0);

    cptr1 = std::move(cptr2);

    REQUIRE(DerivedType::object_count == 0);

    THEN("The move-assigned-from object is empty")
    {
      REQUIRE(!cptr2);
    }

    THEN("The move-assigned-to object is empty")
    {
      REQUIRE(!cptr1);
    }
  }

  GIVEN("A default-constructed copy_on_write move-assigned to a pointer-constructed copy_on_write")
  {
    int v1 = 7;

    copy_on_write<BaseType> cptr1(new DerivedType(v1));
    copy_on_write<BaseType> cptr2;
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 1);

    cptr1 = std::move(cptr2);

    REQUIRE(DerivedType::object_count == 0);

    THEN("The move-assigned-from object is empty")
    {
      REQUIRE(!cptr2);
    }

    THEN("The move-assigned-to object is empty")
    {
      REQUIRE(!cptr1);
    }
  }

  GIVEN("A pointer-constructed copy_on_write move-assigned to a default-constructed copy_on_write")
  {
    int v1 = 7;

    copy_on_write<BaseType> cptr1;
    copy_on_write<BaseType> cptr2(new DerivedType(v1));
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 1);

    cptr1 = std::move(cptr2);

    REQUIRE(DerivedType::object_count == 1);

    THEN("The move-assigned-from object is empty")
    {
      REQUIRE(!cptr2);
    }

    THEN("The move-assigned-to object pointer is the move-assigned-from pointer")
    {
      REQUIRE(&cptr1.value() == p);
    }
  }

  GIVEN("A pointer-constructed copy_on_write move-assigned to a pointer-constructed copy_on_write")
  {
    int v1 = 7;
    int v2 = 87;

    copy_on_write<BaseType> cptr1(new DerivedType(v1));
    copy_on_write<BaseType> cptr2(new DerivedType(v2));
    const auto p = &cptr2.value();

    REQUIRE(DerivedType::object_count == 2);

    cptr1 = std::move(cptr2);

    REQUIRE(DerivedType::object_count == 1);

    THEN("The move-assigned-from object is empty")
    {
      REQUIRE(!cptr2);
    }

    THEN("The move-assigned-to object pointer is the move-assigned-from pointer")
    {
      REQUIRE(&cptr1.value() == p);
    }
  }
}

TEST_CASE("Derived types", "[copy_on_write.derived_types]")
{
  GIVEN("A copy_on_write<BaseType> constructed from make_copy_on_write<DerivedType>")
  {
    int v = 7;
    auto cptr = make_copy_on_write<DerivedType>(v);

    WHEN("A copy_on_write<BaseType> is copy-constructed")
    {
      copy_on_write<BaseType> bptr(cptr);

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A copy_on_write<BaseType> is assigned")
    {
      copy_on_write<BaseType> bptr;
      bptr = cptr;

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A copy_on_write<BaseType> is move-constructed")
    {
      copy_on_write<BaseType> bptr(std::move(cptr));

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }

    WHEN("A copy_on_write<BaseType> is move-assigned")
    {
      copy_on_write<BaseType> bptr;
      bptr = std::move(cptr);

      THEN("Operator-> calls the pointee method")
      {
        REQUIRE(bptr->value() == v);
      }

      THEN("operator bool returns true")
      {
        REQUIRE((bool)bptr == true);
      }
    }
  }
}

TEST_CASE("make_copy_on_write return type can be converted to base-type", "[copy_on_write.make_copy_on_write]")
{
  GIVEN("A copy_on_write<BaseType> constructed from make_copy_on_write<DerivedType>")
  {
    int v = 7;
    copy_on_write<BaseType> cptr = make_copy_on_write<DerivedType>(v);

    THEN("Operator-> calls the pointee method")
    {
      REQUIRE(cptr->value() == v);
    }

    THEN("operator bool returns true")
    {
      REQUIRE((bool)cptr == true);
    }
  }
}

struct Base { int v_ = 42; virtual ~Base() = default; };
struct IntermediateBaseA : virtual Base { int a_ = 3; };
struct IntermediateBaseB : virtual Base { int b_ = 101; };
struct MultiplyDerived : IntermediateBaseA, IntermediateBaseB { int value_ = 0; MultiplyDerived(int value) : value_(value) {}; };

TEST_CASE("Gustafsson's dilemma: multiple (virtual) base classes", "[copy_on_write.constructors]")
{
  GIVEN("A value-constructed multiply-derived-class copy_on_write")
  {
    int v = 7;
    copy_on_write<MultiplyDerived> cptr(new MultiplyDerived(v));

    THEN("When copied to a copy_on_write to an intermediate base type, data is accessible as expected")
    {
      copy_on_write<IntermediateBaseA> cptr_IA = cptr;
      REQUIRE(cptr_IA->a_ == 3);
      REQUIRE(cptr_IA->v_ == 42);
    }

    THEN("When copied to a copy_on_write to an intermediate base type, data is accessible as expected")
    {
      copy_on_write<IntermediateBaseB> cptr_IB = cptr;
      REQUIRE(cptr_IB->b_ == 101);
      REQUIRE(cptr_IB->v_ == 42);
    }
  }
}

struct Tracked
{
  static int ctor_count_;
  static int dtor_count_;
  static int assignment_count_;

  static void reset_counts()
  {
    ctor_count_ = 0;
    dtor_count_ = 0;
    assignment_count_ = 0;
  }

  Tracked() { ++ctor_count_; }
  ~Tracked() { ++dtor_count_; }
  Tracked(const Tracked&) { ++ctor_count_; }
  Tracked(Tracked&&) { ++ctor_count_; }
  Tracked& operator=(const Tracked&) { ++assignment_count_; return *this; }
  Tracked& operator=(Tracked&&) { ++assignment_count_; return *this; }
};

int Tracked::ctor_count_ = 0;
int Tracked::dtor_count_ = 0;
int Tracked::assignment_count_ = 0;

struct ThrowsOnCopy : Tracked
{
  int value_ = 0;

  ThrowsOnCopy() = default;

  explicit ThrowsOnCopy(const int v) : value_(v) {}

  ThrowsOnCopy(const ThrowsOnCopy&)
  {
    throw std::runtime_error("something went wrong during copy");
  }

  ThrowsOnCopy& operator=(const ThrowsOnCopy& rhs) = default;
};

TEST_CASE("Exception safety: throw in copy constructor", "[copy_on_write.exception_safety.copy]")
{
  GIVEN("A value-constructed copy_on_write to a ThrowsOnCopy")
  {
    const int v = 7;
    copy_on_write<ThrowsOnCopy> cptr(new ThrowsOnCopy(v));

    THEN("When copying to another copy_on_write, after an exception, the source remains valid")
    {
      Tracked::reset_counts();
      REQUIRE_THROWS_AS(copy_on_write<ThrowsOnCopy> another = cptr, std::runtime_error);
      REQUIRE(cptr->value_ == v);
      REQUIRE(Tracked::ctor_count_ - Tracked::dtor_count_ == 0);
    }

    THEN("When copying to another copy_on_write, after an exception, the destination is not changed")
    {
      const int v2 = 5;
      copy_on_write<ThrowsOnCopy> another(new ThrowsOnCopy(v2));
      Tracked::reset_counts();
      REQUIRE_THROWS_AS(another = cptr, std::runtime_error);
      REQUIRE(another->value_ == v2);
      REQUIRE(Tracked::ctor_count_ - Tracked::dtor_count_ == 0);
    }
  }
}

template <typename T>
struct throwing_copier
{
  T* operator()(const T& t) const
  {
    throw std::bad_alloc{};
  }
};

struct TrackedValue : Tracked
{
  int value_ = 0;
  explicit TrackedValue(const int v) : value_(v) {}
};

TEST_CASE("Exception safety: throw in copier", "[copy_on_write.exception_safety.copier]")
{
  GIVEN("A value-constructed copy_on_write")
  {
    const int v = 7;
    copy_on_write<TrackedValue> cptr(new TrackedValue(v), throwing_copier<TrackedValue>{});

    THEN("When an exception occurs in the copier, the source is unchanged")
    {
      copy_on_write<TrackedValue> another;
      Tracked::reset_counts();
      REQUIRE_THROWS_AS(another = cptr, std::bad_alloc);
      REQUIRE(cptr->value_ == v);
      REQUIRE(Tracked::ctor_count_ - Tracked::dtor_count_ == 0);
    }

    THEN("When an exception occurs in the copier, the destination is unchanged")
    {
      const int v2 = 5;
      copy_on_write<TrackedValue> another(new TrackedValue(v2));
      Tracked::reset_counts();
      REQUIRE_THROWS_AS(another = cptr, std::bad_alloc);
      REQUIRE(another->value_ == v2);
      REQUIRE(Tracked::ctor_count_ - Tracked::dtor_count_ == 0);
    }
  }
}
