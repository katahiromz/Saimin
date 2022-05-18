#pragma once

#include <stdlib.h>
#include <stdio.h>

#ifndef _WIN32
#define __stdcall
#elif defined(__BORLANDC__)
#define _MSC_VER 0
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

extern GLdouble vertices[512][6];
extern int vertexIndex;

#ifndef CBACK
#define CBACK
#endif

static inline int tessBegin(void)
{
    return vertexIndex;
}
static inline int tessEnd(void)
{
    return vertexIndex;
}

void tessVertex(GLdouble x, GLdouble y, GLdouble z, GLdouble r, GLdouble g, GLdouble b);

void tessBezier3(GLdouble x0, GLdouble y0, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2,
                 GLdouble x3, GLdouble y3, GLdouble r, GLdouble g, GLdouble b, int count);

GLuint tess(size_t num, GLdouble *data, bool fill = true);

void CBACK tessBeginCB(GLenum which);
void CBACK tessEndCB(void);
void CBACK tessErrorCB(GLenum errorCode);
void CBACK tessVertexCB(const GLvoid *data);
void CBACK tessVertexCB2(const GLvoid *data);
void CBACK tessCombineCB(const GLdouble newVertex[3], const GLdouble *neighborVertex[4],
                         const GLfloat neighborWeight[4], GLdouble **outData);
