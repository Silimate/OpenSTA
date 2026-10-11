module top (in, out1, out2);
  input in;
  output out1, out2;
  wire n1;
  BUF_X1 u1 (.A(in), .Z(n1));
  BUF_X1 u2 (.A(n1), .Z(out1));
  BUF_X16 u3 (.A(n1), .Z(out2));
endmodule
