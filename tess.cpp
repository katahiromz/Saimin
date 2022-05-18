#include "tess.h"

GLdouble vertices[512][6];
int vertexIndex = 0;

void tessVertex(GLdouble x, GLdouble y, GLdouble z, GLdouble r, GLdouble g, GLdouble b)
{
    vertices[vertexIndex][0] = x;
    vertices[vertexIndex][1] = y;
    vertices[vertexIndex][2] = z;
    vertices[vertexIndex][3] = r;
    vertices[vertexIndex][4] = g;
    vertices[vertexIndex][5] = b;
    vertexIndex++;
}

void tessBezier3(
    GLdouble x0, GLdouble y0,
    GLdouble x1, GLdouble y1,
    GLdouble x2, GLdouble y2,
    GLdouble x3, GLdouble y3,
    GLdouble r, GLdouble g, GLdouble b, int count)
{
    if (count <= 1)
        count = 16;
    int i = 0;
    tessVertex(x0, y0, 0, r, g, b);
    for (++i; i < (count - 1); ++i)
    {
        GLdouble t = float(i) / (count - 1);
        GLdouble t_2 = t * t;
        GLdouble t_3 = t_2 * t;
        GLdouble one_minus_t = 1 - t;
        GLdouble one_minus_t_2 = one_minus_t * one_minus_t;
        GLdouble one_minus_t_3 = one_minus_t_2 * one_minus_t;
        GLdouble x = (one_minus_t_3 * x0) + (3 * one_minus_t_2 * t * x1) + (3 * one_minus_t * t_2 * x2) + (t_3 * x3);
        GLdouble y = (one_minus_t_3 * y0) + (3 * one_minus_t_2 * t * y1) + (3 * one_minus_t * t_2 * y2) + (t_3 * y3);
        tessVertex(x, y, 0, r, g, b);
    }
    tessVertex(x3, y3, 0, r, g, b);
}

void CBACK tessBeginCB(GLenum which)
{
    glBegin(which);
}

void CBACK tessVertexCB2(const GLvoid *data)
{
    const GLdouble *ptr = (const GLdouble*)data;

    glColor3dv(ptr + 3);
    glVertex3dv(ptr);
}

void CBACK tessVertexCB(const GLvoid *data)
{
    const GLdouble *ptr = (const GLdouble*)data;

    glVertex3dv(ptr);
}

void CBACK tessEndCB()
{
    glEnd();
    vertexIndex = 0;
}

void CBACK tessCombineCB(const GLdouble newVertex[3], const GLdouble *neighborVertex[4],
                            const GLfloat neighborWeight[4], GLdouble **outData)
{
    vertices[vertexIndex][0] = newVertex[0];
    vertices[vertexIndex][1] = newVertex[1];
    vertices[vertexIndex][2] = newVertex[2];

    vertices[vertexIndex][3] = neighborWeight[0] * neighborVertex[0][3] +   // red
                               neighborWeight[1] * neighborVertex[1][3] +
                               neighborWeight[2] * neighborVertex[2][3] +
                               neighborWeight[3] * neighborVertex[3][3];
    vertices[vertexIndex][4] = neighborWeight[0] * neighborVertex[0][4] +   // green
                               neighborWeight[1] * neighborVertex[1][4] +
                               neighborWeight[2] * neighborVertex[2][4] +
                               neighborWeight[3] * neighborVertex[3][4];
    vertices[vertexIndex][5] = neighborWeight[0] * neighborVertex[0][5] +   // blue
                               neighborWeight[1] * neighborVertex[1][5] +
                               neighborWeight[2] * neighborVertex[2][5] +
                               neighborWeight[3] * neighborVertex[3][5];

    *outData = vertices[vertexIndex];
    ++vertexIndex;
}

void CBACK tessErrorCB(GLenum errorCode)
{
}

GLuint tess(size_t num, GLdouble *data, bool fill)
{
    GLuint id = glGenLists(1);
    if (!id)
        return 0;

    GLUtesselator *tess = gluNewTess();
    if (!tess)
        return 0;

    gluTessCallback(tess, GLU_TESS_BEGIN, (void (__stdcall*)(void))tessBeginCB);
    gluTessCallback(tess, GLU_TESS_END, (void (__stdcall*)(void))tessEndCB);
    gluTessCallback(tess, GLU_TESS_ERROR, (void (__stdcall*)(void))tessErrorCB);
    gluTessCallback(tess, GLU_TESS_VERTEX, (void (__stdcall*)(void))tessVertexCB2);
    gluTessCallback(tess, GLU_TESS_COMBINE, (void (__stdcall*)(void))tessCombineCB);

    gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
    gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, fill ? GL_FALSE : GL_TRUE);
    glNewList(id, GL_COMPILE);
    gluTessBeginPolygon(tess, 0);
        gluTessBeginContour(tess);
            for (size_t i = 0; i < num; ++i)
                gluTessVertex(tess, &data[i * 6], &data[i * 6]);
        gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    glEndList();

    gluDeleteTess(tess);

    return id;
}
