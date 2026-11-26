#include "xsdk_input.h"
#include <xsdk/kernel.h>
#include <cstring>
static unsigned char buttons[15];
static float axes[6];
static bool testPadEnabled=false;
static GLFWgamepadstate testPad;
void lcs_test_set_pad(const GLFWgamepadstate *s) {testPadEnabled=s!=nullptr;if(s)testPad=*s;}
static int poll(int jid) {
 if(jid==0 && testPadEnabled) {std::memcpy(buttons,testPad.buttons,sizeof buttons);std::memcpy(axes,testPad.axes,sizeof axes);return 1;}
 std::memset(buttons,0,sizeof buttons); std::memset(axes,0,sizeof axes);
 INPUT_STATE state={};
 if(jid!=0 || XamInputGetState(0,0,&state)) return 0;
 const unsigned short bits[]={INPUT_GAMEPAD_A,INPUT_GAMEPAD_B,INPUT_GAMEPAD_X,INPUT_GAMEPAD_Y,
 INPUT_GAMEPAD_LEFT_SHOULDER,INPUT_GAMEPAD_RIGHT_SHOULDER,INPUT_GAMEPAD_BACK,INPUT_GAMEPAD_START,
 0,INPUT_GAMEPAD_LEFT_THUMB,INPUT_GAMEPAD_RIGHT_THUMB,INPUT_GAMEPAD_DPAD_UP,
 INPUT_GAMEPAD_DPAD_RIGHT,INPUT_GAMEPAD_DPAD_DOWN,INPUT_GAMEPAD_DPAD_LEFT};
 for(int i=0;i<15;i++) buttons[i]=(state.gamepad.buttons&bits[i])!=0;
 auto axis=[](short n){return n<0?n/32768.0f:n/32767.0f;};
 axes[0]=axis(state.gamepad.thumb_lx);axes[1]=-axis(state.gamepad.thumb_ly);
 axes[2]=axis(state.gamepad.thumb_rx);axes[3]=-axis(state.gamepad.thumb_ry);
 axes[4]=state.gamepad.left_trigger*(2.0f/255.0f)-1.0f;
 axes[5]=state.gamepad.right_trigger*(2.0f/255.0f)-1.0f;
 return 1;
}
void glfwGetCursorPos(GLFWwindow*,double*x,double*y){if(x)*x=0;if(y)*y=0;}
void glfwSetCursorPos(GLFWwindow*,double,double){}
int glfwGetMouseButton(GLFWwindow*,int){return 0;}
int glfwGetKey(GLFWwindow*,int){return 0;}
void glfwSetInputMode(GLFWwindow*,int,int){}
int glfwJoystickPresent(int jid){return poll(jid);}
const char*glfwGetJoystickName(int jid){return jid==0?"Xbox 360 Controller":nullptr;}
const unsigned char*glfwGetJoystickButtons(int jid,int*count){int ok=poll(jid);if(count)*count=ok?15:0;return ok?buttons:nullptr;}
const float*glfwGetJoystickAxes(int jid,int*count){int ok=poll(jid);if(count)*count=ok?6:0;return ok?axes:nullptr;}
int glfwJoystickIsGamepad(int jid){return poll(jid);}
int glfwGetGamepadState(int jid,GLFWgamepadstate*out){if(!out||!poll(jid))return 0;std::memcpy(out->buttons,buttons,sizeof buttons);std::memcpy(out->axes,axes,sizeof axes);return 1;}