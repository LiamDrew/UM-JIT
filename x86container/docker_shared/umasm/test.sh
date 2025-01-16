
echo "Testing print six"
output=$(../jit print-six.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing simple mult"
output=$(../jit simple_mult.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Hello World"
output=$(../jit hello.um)
expected=$(printf "Hello, world.\n")
if [ "$output" = "$expected" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Conditional Move Yes"
output=$(../jit condmove_yes.um)
if [ "$output" = "7" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Conditional Move No"
output=$(../jit condmove_no.um)
if [ "$output" = "6" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi


echo "Testing Echo"
output=$(../jit echo.um <<< 8)
if [ "$output" = "8" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi

echo "Testing Nand"
output=$(../jit nand.um)
if [ "$output" = "7" ]; then
  echo "Test passed"
else
  echo "Test failed. Got: $output"
fi