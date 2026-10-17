struct Gamepad : Controller {
  enum : uint {
    Up, Down, Left, Right, B, A, Y, X, L, R, Select, Start,
  };

  Gamepad(uint port);

  auto data() -> uint2;
  auto diagnosticSerialize(serializer& s) -> void {
    s.boolean(latched); s.integer(counter);
    s.boolean(b); s.boolean(y); s.boolean(select); s.boolean(start);
    s.boolean(up); s.boolean(down); s.boolean(left); s.boolean(right);
    s.boolean(a); s.boolean(x); s.boolean(l); s.boolean(r);
  }
  auto latch(bool data) -> void;

private:
  bool latched;
  uint counter;

  boolean b, y, select, start;
  boolean up, down, left, right;
  boolean a, x, l, r;
};
