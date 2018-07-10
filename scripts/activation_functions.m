%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%   Copyright 2018 - The OPRECOMP Project Consortium,
%                    IBM Research GmbH. All rights reserved.
%
%   Licensed under the Apache License, Version 2.0 (the "License");
%   you may not use this file except in compliance with the License.
%   You may obtain a copy of the License at
%
%       http://www.apache.org/licenses/LICENSE-2.0
%
%   Unless required by applicable law or agreed to in writing, software
%   distributed under the License is distributed on an "AS IS" BASIS,
%   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%   See the License for the specific language governing permissions and
%   limitations under the License.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% @file activation_functions.m
% @author Dionysios Diamantopoulos, did@zurich.ibm.com
% @date 15 May 2018
% @brief Script for approximating activation functions of BLSTM with piecewise
% linear techniques.

MIN_TARGET_EXPF = -10;
MAX_TARGET_EXPF = 10;
LUT_SIZE_EXPF = 5;
SIZE_EXPF = 10000;
PWL_SIZE_EXPF = 6;

step_lut = (abs(MAX_TARGET_EXPF) + abs(MIN_TARGET_EXPF)) / (LUT_SIZE_EXPF-1);
stepf = (abs(MAX_TARGET_EXPF) + abs(MIN_TARGET_EXPF)) / (SIZE_EXPF-1);
step_pwl = (abs(MAX_TARGET_EXPF) + abs(MIN_TARGET_EXPF)) / (PWL_SIZE_EXPF-1);

%x_lut = MIN_TARGET_EXPF:step_lut:(MAX_TARGET_EXPF+MIN_TARGET_EXPF)/2;
%x_lut = [x_lut,(MAX_TARGET_EXPF+MIN_TARGET_EXPF)/2:step_lut/2:MAX_TARGET_EXPF];

x_lut = MIN_TARGET_EXPF:step_lut:MAX_TARGET_EXPF;
%x_lut = MIN_TARGET_EXPF:step_lut:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF;
%x_lut = [x_lut,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:step_lut/2:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF];
%x_lut = [x_lut,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:step_lut/4:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/8+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF];
%x_lut = [x_lut,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/8+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:step_lut/8:MAX_TARGET_EXPF];

x_pwl1 = MIN_TARGET_EXPF:step_pwl:MAX_TARGET_EXPF;
x_pwl2 = MIN_TARGET_EXPF:step_pwl:-2;
x_pwl2 = [x_pwl2,-2:step_pwl/2:-1];
x_pwl2 = [x_pwl2,-1:step_pwl/4:1];
x_pwl2 = [x_pwl2,1:step_pwl/2:2];
x_pwl2 = [x_pwl2,2:step_pwl:MAX_TARGET_EXPF];
%x_pwl2 = MIN_TARGET_EXPF:step_pwl:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF;
%x_pwl2 = [x_pwl2,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:step_pwl/2:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF];
%x_pwl2 = [x_pwl2,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:step_pwl/4:MAX_TARGET_EXPF];

x_lut_extend = reshape(repmat(x_lut,SIZE_EXPF/LUT_SIZE_EXPF,1),1,[]);

%x_f = MIN_TARGET_EXPF:stepf:(MAX_TARGET_EXPF+MIN_TARGET_EXPF)/2;
%x_f = [x_f,(MAX_TARGET_EXPF+MIN_TARGET_EXPF)/2:stepf/2:MAX_TARGET_EXPF];

x_f = MIN_TARGET_EXPF:stepf:MAX_TARGET_EXPF;
%x_f = MIN_TARGET_EXPF:stepf:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF;
%x_f = [x_f,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:stepf/2:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF];
%x_f = [x_f,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:stepf/4:(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/8+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF];
%x_f = [x_f,(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/8+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/4+(MAX_TARGET_EXPF-MIN_TARGET_EXPF)/2+MIN_TARGET_EXPF:stepf/8:MAX_TARGET_EXPF];



function y = func (x)
  %y = exp(x);
  y = tanh(x);
  %y = 1.0./(1.0 + exp(-x));
endfunction

expf_lut = func(x_lut);
expf_lut_extend = func(x_lut_extend);

expf = func(x_f);

subplot (4, 2, 1)
plot (x_f, expf, 'b-');
hold on;
plot (x_f, expf_lut_extend,'r-');
%plot (x_f, expf_lut_extend,'r-', x_lut, expf_lut,'go');
subplot (4, 2, 3)
semilogy (x_f, expf, 'b-');
hold on;
semilogy (x_f, expf_lut_extend,'r-');
%semilogy (x_f, expf_lut_extend,'r-', x_lut, expf_lut,'go');
subplot (4, 2, 5)
plot (x_f, abs(expf_lut_extend-expf), 'b-');
subplot (4, 2, 7)
semilogy (x_f, abs(expf_lut_extend-expf), 'b-');


ys1=func(x_pwl1);
ys2=func(x_pwl2);
y1=interp1(x_pwl1,ys1,x_f,'linear');
y2=interp1(x_pwl2,ys2,x_f,'linear');
subplot (4, 2, 2)
plot(x_f,y1,'-',x_f,y2,'-', x_pwl1,ys1,'go', x_pwl2,ys2,'ro',x_f,expf,'-')
subplot (4, 2, 4)
semilogy(x_f,y1,'-',x_pwl1,ys1,'go',x_f,expf,'-')
subplot (4, 2, 6)
y1err = abs(y1-expf);
y2err = abs(y2-expf);
plot(x_f,y1err,'-', x_f,y2err,'-');
hold on;
plot(x_f, linspace(y1errm, y1errm, length(x_f)),'--', x_f, linspace(y2errm, y2errm, length(x_f)),'--');
hold off;
subplot (4, 2, 8)
semilogy(x_f,y1err,'-', x_f,y2err,'-')
hold on;
y1errm = mean(y1err);
y2errm = mean(y2err);
plot(x_f, linspace(y1errm, y1errm, length(x_f)),'--', x_f, linspace(y2errm, y2errm, length(x_f)),'--');
hold off;
