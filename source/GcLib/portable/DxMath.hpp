#pragma once
#include "Win32Compat.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

struct D3DXVECTOR2 {
    float x = 0, y = 0;
    D3DXVECTOR2() = default;
    D3DXVECTOR2(float x, float y) : x(x), y(y) {}
    explicit D3DXVECTOR2(const float* p) : x(p[0]), y(p[1]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    D3DXVECTOR2& operator+=(const D3DXVECTOR2& b) { x += b.x; y += b.y; return *this; }
    D3DXVECTOR2& operator-=(const D3DXVECTOR2& b) { x -= b.x; y -= b.y; return *this; }
    D3DXVECTOR2& operator*=(float s) { x *= s; y *= s; return *this; }
};
struct D3DVECTOR { float x = 0, y = 0, z = 0; };
struct D3DXVECTOR3 : D3DVECTOR {
    D3DXVECTOR3() = default;
    D3DXVECTOR3(float x, float y, float z) { this->x=x; this->y=y; this->z=z; }
    D3DXVECTOR3(const D3DVECTOR& v) { x=v.x; y=v.y; z=v.z; }
    explicit D3DXVECTOR3(const float* p) { x=p[0]; y=p[1]; z=p[2]; }
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& b) { x-=b.x; y-=b.y; z-=b.z; return *this; }
    D3DXVECTOR3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
};
struct D3DXVECTOR4 {
    float x=0, y=0, z=0, w=0;
    D3DXVECTOR4() = default;
    D3DXVECTOR4(float x,float y,float z,float w) : x(x),y(y),z(z),w(w) {}
    D3DXVECTOR4(const D3DXVECTOR3& v,float w) : x(v.x),y(v.y),z(v.z),w(w) {}
    explicit D3DXVECTOR4(const float* p) : x(p[0]),y(p[1]),z(p[2]),w(p[3]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
};
struct D3DXQUATERNION {
    float x=0,y=0,z=0,w=1;
    D3DXQUATERNION() = default;
    D3DXQUATERNION(float x,float y,float z,float w) : x(x),y(y),z(z),w(w) {}
};
struct D3DXPLANE {
    float a=0,b=0,c=0,d=0;
    D3DXPLANE()=default;
    D3DXPLANE(float a,float b,float c,float d):a(a),b(b),c(c),d(d){}
};
struct D3DMATRIX {
    union {
        struct { float _11,_12,_13,_14, _21,_22,_23,_24, _31,_32,_33,_34, _41,_42,_43,_44; };
        float m[4][4];
    };
};
struct D3DXMATRIX : D3DMATRIX {
    D3DXMATRIX() = default;
    explicit D3DXMATRIX(const float* p) { std::memcpy(m,p,sizeof(m)); }
    D3DXMATRIX(const D3DMATRIX& a) { std::memcpy(m,a.m,sizeof(m)); }
    D3DXMATRIX(float a,float b,float c,float d,float e,float f,float g,float h,
               float i,float j,float k,float l,float n,float o,float p,float q) {
        const float values[]={a,b,c,d,e,f,g,h,i,j,k,l,n,o,p,q}; std::memcpy(m,values,sizeof(m));
    }
    operator float*() { return &m[0][0]; }
    operator const float*() const { return &m[0][0]; }
    float& operator()(unsigned row,unsigned column) { return m[row][column]; }
    float operator()(unsigned row,unsigned column) const { return m[row][column]; }
    D3DXMATRIX& operator*=(const D3DXMATRIX& other);
};
inline D3DXVECTOR2 operator+(D3DXVECTOR2 a,const D3DXVECTOR2& b){return a+=b;}
inline D3DXVECTOR2 operator-(D3DXVECTOR2 a,const D3DXVECTOR2& b){return a-=b;}
inline D3DXVECTOR2 operator*(D3DXVECTOR2 a,float s){return a*=s;}
inline D3DXVECTOR2 operator/(D3DXVECTOR2 a,float s){return a*=1/s;}
inline D3DXVECTOR3 operator+(D3DXVECTOR3 a,const D3DXVECTOR3& b){return a+=b;}
inline D3DXVECTOR3 operator-(D3DXVECTOR3 a,const D3DXVECTOR3& b){return a-=b;}
inline D3DXVECTOR3 operator-(const D3DXVECTOR3& a){return {-a.x,-a.y,-a.z};}
inline D3DXVECTOR3 operator*(D3DXVECTOR3 a,float s){return a*=s;}
inline D3DXVECTOR3 operator*(float s,D3DXVECTOR3 a){return a*=s;}
inline D3DXVECTOR3 operator/(D3DXVECTOR3 a,float s){return a*=1/s;}
inline D3DXVECTOR4 operator+(const D3DXVECTOR4& a,const D3DXVECTOR4& b){return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w};}
inline D3DXVECTOR4 operator*(const D3DXVECTOR4& a,float s){return {a.x*s,a.y*s,a.z*s,a.w*s};}
inline D3DXMATRIX operator*(const D3DXMATRIX& a,const D3DXMATRIX& b) {
    D3DXMATRIX result;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j){result.m[i][j]=0;for(int k=0;k<4;++k)result.m[i][j]+=a.m[i][k]*b.m[k][j];}
    return result;
}
inline D3DXMATRIX& D3DXMATRIX::operator*=(const D3DXMATRIX& b){return *this=*this*b;}
inline float D3DXToRadian(float degrees){return degrees*0.01745329251994329577f;}
inline float D3DXToDegree(float radians){return radians*57.295779513082320877f;}
inline float D3DXVec2Dot(const D3DXVECTOR2* a,const D3DXVECTOR2* b){return a->x*b->x+a->y*b->y;}
inline float D3DXVec2CCW(const D3DXVECTOR2* a,const D3DXVECTOR2* b){return a->x*b->y-a->y*b->x;}
inline float D3DXVec2Length(const D3DXVECTOR2* a){return std::sqrt(D3DXVec2Dot(a,a));}
inline float D3DXVec3Dot(const D3DXVECTOR3* a,const D3DXVECTOR3* b){return a->x*b->x+a->y*b->y+a->z*b->z;}
inline float D3DXVec3LengthSq(const D3DXVECTOR3* a){return D3DXVec3Dot(a,a);}
inline float D3DXVec3Length(const D3DXVECTOR3* a){return std::sqrt(D3DXVec3LengthSq(a));}
inline D3DXVECTOR2* D3DXVec2Normalize(D3DXVECTOR2* out,const D3DXVECTOR2* a){float l=D3DXVec2Length(a);*out=l>0?*a/l:D3DXVECTOR2{};return out;}
inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out,const D3DXVECTOR3* a){float l=D3DXVec3Length(a);*out=l>0?*a/l:D3DXVECTOR3{};return out;}
inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b){*out={a->y*b->z-a->z*b->y,a->z*b->x-a->x*b->z,a->x*b->y-a->y*b->x};return out;}
inline D3DXVECTOR3* D3DXVec3Add(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b){*out=*a+*b;return out;}
inline D3DXVECTOR3* D3DXVec3Subtract(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b){*out=*a-*b;return out;}
inline D3DXVECTOR3* D3DXVec3Scale(D3DXVECTOR3* out,const D3DXVECTOR3* a,float s){*out=*a*s;return out;}
inline D3DXVECTOR3* D3DXVec3Lerp(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b,float t){*out=*a+(*b-*a)*t;return out;}
inline D3DXVECTOR4* D3DXVec4Transform(D3DXVECTOR4* out,const D3DXVECTOR4* a,const D3DXMATRIX* m){D3DXVECTOR4 r;for(int i=0;i<4;++i)(&r.x)[i]=a->x*m->m[0][i]+a->y*m->m[1][i]+a->z*m->m[2][i]+a->w*m->m[3][i];*out=r;return out;}
inline D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXMATRIX* m){D3DXVECTOR4 v(*a,1);D3DXVec4Transform(&v,&v,m);*out={v.x/v.w,v.y/v.w,v.z/v.w};return out;}
inline D3DXVECTOR3* D3DXVec3TransformNormal(D3DXVECTOR3* out,const D3DXVECTOR3* a,const D3DXMATRIX* m){D3DXVECTOR4 v(*a,0);D3DXVec4Transform(&v,&v,m);*out={v.x,v.y,v.z};return out;}
inline D3DXVECTOR3* D3DXVec3Hermite(D3DXVECTOR3* out,const D3DXVECTOR3* p1,const D3DXVECTOR3* t1,const D3DXVECTOR3* p2,const D3DXVECTOR3* t2,float t){float t2v=t*t,t3=t2v*t;*out=*p1*(2*t3-3*t2v+1)+*t1*(t3-2*t2v+t)+*p2*(-2*t3+3*t2v)+*t2*(t3-t2v);return out;}
inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* out){std::memset(out,0,sizeof(*out));out->_11=out->_22=out->_33=out->_44=1;return out;}
inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* out,float x,float y,float z){D3DXMatrixIdentity(out);out->_11=x;out->_22=y;out->_33=z;return out;}
inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* out,float x,float y,float z){D3DXMatrixIdentity(out);out->_41=x;out->_42=y;out->_43=z;return out;}
inline D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out,const D3DXMATRIX* a,const D3DXMATRIX* b){*out=*a**b;return out;}
inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out,const D3DXMATRIX* a){D3DXMATRIX r;for(int i=0;i<4;++i)for(int j=0;j<4;++j)r.m[i][j]=a->m[j][i];*out=r;return out;}
inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out,float* determinant,const D3DXMATRIX* a){
    double augmented[4][8]{};double det=1;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j){augmented[i][j]=a->m[i][j];augmented[i][j+4]=i==j;}
    for(int column=0;column<4;++column){
        int pivot=column;for(int row=column+1;row<4;++row)if(std::abs(augmented[row][column])>std::abs(augmented[pivot][column]))pivot=row;
        if(augmented[pivot][column]==0){if(determinant)*determinant=0;return nullptr;}
        if(pivot!=column){for(int j=0;j<8;++j)std::swap(augmented[pivot][j],augmented[column][j]);det=-det;}
        double divisor=augmented[column][column];det*=divisor;for(double& v:augmented[column])v/=divisor;
        for(int row=0;row<4;++row)if(row!=column){double factor=augmented[row][column];for(int j=0;j<8;++j)augmented[row][j]-=factor*augmented[column][j];}
    }
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)out->m[i][j]=float(augmented[i][j+4]);if(determinant)*determinant=float(det);return out;
}
inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* out,float a){D3DXMatrixIdentity(out);out->_22=out->_33=std::cos(a);out->_23=std::sin(a);out->_32=-std::sin(a);return out;}
inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* out,float a){D3DXMatrixIdentity(out);out->_11=out->_33=std::cos(a);out->_31=std::sin(a);out->_13=-std::sin(a);return out;}
inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out,float a){D3DXMatrixIdentity(out);out->_11=out->_22=std::cos(a);out->_12=std::sin(a);out->_21=-std::sin(a);return out;}
inline D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX* out,float yaw,float pitch,float roll){D3DXMATRIX x,y,z;D3DXMatrixRotationX(&x,pitch);D3DXMatrixRotationY(&y,yaw);D3DXMatrixRotationZ(&z,roll);*out=z*x*y;return out;}
inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* out,const D3DXVECTOR3* eye,const D3DXVECTOR3* at,const D3DXVECTOR3* up){D3DXVECTOR3 z=*at-*eye,x,y;D3DXVec3Normalize(&z,&z);D3DXVec3Cross(&x,up,&z);D3DXVec3Normalize(&x,&x);D3DXVec3Cross(&y,&z,&x);*out={x.x,y.x,z.x,0,x.y,y.y,z.y,0,x.z,y.z,z.z,0,-D3DXVec3Dot(&x,eye),-D3DXVec3Dot(&y,eye),-D3DXVec3Dot(&z,eye),1};return out;}
inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* out,float fov,float aspect,float nearZ,float farZ){std::memset(out,0,sizeof(*out));float ys=1/std::tan(fov*.5f);out->_11=ys/aspect;out->_22=ys;out->_33=farZ/(farZ-nearZ);out->_34=1;out->_43=-nearZ*farZ/(farZ-nearZ);return out;}
inline D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* out,float width,float height,float nearZ,float farZ){D3DXMatrixIdentity(out);out->_11=2/width;out->_22=2/height;out->_33=1/(farZ-nearZ);out->_43=-nearZ/(farZ-nearZ);return out;}
inline D3DXQUATERNION* D3DXQuaternionRotationYawPitchRoll(D3DXQUATERNION* out,float yaw,float pitch,float roll){float sy=std::sin(yaw/2),cy=std::cos(yaw/2),sp=std::sin(pitch/2),cp=std::cos(pitch/2),sr=std::sin(roll/2),cr=std::cos(roll/2);*out={cy*sp*cr+sy*cp*sr,sy*cp*cr-cy*sp*sr,cy*cp*sr-sy*sp*cr,cy*cp*cr+sy*sp*sr};return out;}
inline D3DXQUATERNION* D3DXQuaternionMultiply(D3DXQUATERNION* out,const D3DXQUATERNION* a,const D3DXQUATERNION* b){*out={b->w*a->x+b->x*a->w+b->y*a->z-b->z*a->y,b->w*a->y-b->x*a->z+b->y*a->w+b->z*a->x,b->w*a->z+b->x*a->y-b->y*a->x+b->z*a->w,b->w*a->w-b->x*a->x-b->y*a->y-b->z*a->z};return out;}
inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* out,const D3DXQUATERNION* a,const D3DXQUATERNION* b,float t){float dot=a->x*b->x+a->y*b->y+a->z*b->z+a->w*b->w;float sign=dot<0?-1:1;dot=std::abs(dot);float x=1-t,y=t*sign;if(dot<.9995f){float angle=std::acos(std::min(1.f,dot));x=std::sin((1-t)*angle)/std::sin(angle);y=sign*std::sin(t*angle)/std::sin(angle);}*out={a->x*x+b->x*y,a->y*x+b->y*y,a->z*x+b->z*y,a->w*x+b->w*y};return out;}
inline D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* out,const D3DXQUATERNION* q){float x=q->x,y=q->y,z=q->z,w=q->w;*out={1-2*y*y-2*z*z,2*x*y+2*z*w,2*x*z-2*y*w,0,2*x*y-2*z*w,1-2*x*x-2*z*z,2*y*z+2*x*w,0,2*x*z+2*y*w,2*y*z-2*x*w,1-2*x*x-2*y*y,0,0,0,0,1};return out;}
inline D3DXPLANE* D3DXPlaneFromPoints(D3DXPLANE* out,const D3DXVECTOR3* a,const D3DXVECTOR3* b,const D3DXVECTOR3* c){D3DXVECTOR3 ab=*b-*a,ac=*c-*a,n;D3DXVec3Cross(&n,&ab,&ac);D3DXVec3Normalize(&n,&n);*out={n.x,n.y,n.z,-D3DXVec3Dot(&n,a)};return out;}
inline D3DXVECTOR3* D3DXPlaneIntersectLine(D3DXVECTOR3* out,const D3DXPLANE* p,const D3DXVECTOR3* a,const D3DXVECTOR3* b){D3DXVECTOR3 direction=*b-*a;float denominator=p->a*direction.x+p->b*direction.y+p->c*direction.z;if(denominator==0)return nullptr;float t=-(p->a*a->x+p->b*a->y+p->c*a->z+p->d)/denominator;*out=*a+direction*t;return out;}
