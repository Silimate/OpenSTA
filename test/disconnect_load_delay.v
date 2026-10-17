module top (in, out1, out2);
  input in;
  output out1, out2;
  wire n1;
  BUF_X1 u1 (.A(in), .Z(n1));
  BUF_X1 u2 (.A(n1), .Z(out1));
  BUF_X16 u3 (.A(n1), .Z(out2));
endmodule

module top_multi (in, en1, en2, out1, out2);
  input in, en1, en2;
  output out1, out2;
  wire n1;
  TBUF_X1 t1 (.A(in), .EN(en1), .Z(n1));
  TBUF_X1 t2 (.A(in), .EN(en2), .Z(n1));
  BUF_X1 u2 (.A(n1), .Z(out1));
  BUF_X16 u3 (.A(n1), .Z(out2));
endmodule

module top_multi_one_load (in, en1, en2, out1);
  input in, en1, en2;
  output out1;
  wire n1;
  TBUF_X1 t1 (.A(in), .EN(en1), .Z(n1));
  TBUF_X1 t2 (.A(in), .EN(en2), .Z(n1));
  BUF_X1 u2 (.A(n1), .Z(out1));
endmodule
