# A simple test script for running simple UM programs with the JIT.
# (This container was the original development environment for the JIT.)

echo "Testing print six"
output=$(./jit ../umasm/print-six.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing simple mult"
output=$(./jit ../umasm/simple_mult.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Hello World"
output=$(./jit ../umasm/hello.um)
expected=$(printf "Hello, world.\n")
if [ "$output" = "$expected" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Conditional Move Yes"
output=$(./jit ../umasm/condmove_yes.um)
if [ "$output" = "7" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Conditional Move No"
output=$(./jit ../umasm/condmove_no.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Echo"
output=$(./jit ../umasm/echo.um <<< 8)
if [ "$output" = "8" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Nand"
output=$(./jit ../umasm/nand.um)
if [ "$output" = "7" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi