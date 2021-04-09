#define PY_SSIZE_T_CLEAN
#include <Python.h>

int pyfasttextureutils_color_tuple_one_value_to_int(PyObject* tuple, int index, int* val_int) {
  PyObject* val_obj = PyTuple_GetItem(tuple, index);
  if (val_obj == NULL) {
    return 0;
  }
  if (PyNumber_Check(val_obj) != 1) {
    PyErr_SetString(PyExc_TypeError, "Color tuple contains non-numeric object.");
    return 0;
  }
  PyObject* val_num = PyNumber_Long(val_obj);
  *val_int = PyLong_AsUnsignedLong(val_num);
  Py_DECREF(val_num);
  return 1;
}

int pyfasttextureutils_color_tuple_to_rgb(PyObject* color_tuple, int* r, int* g, int*b) {
  if (!pyfasttextureutils_color_tuple_one_value_to_int(color_tuple, 0, r)) {
    return 0;
  }
  if (!pyfasttextureutils_color_tuple_one_value_to_int(color_tuple, 1, g)) {
    return 0;
  }
  if (!pyfasttextureutils_color_tuple_one_value_to_int(color_tuple, 2, b)) {
    return 0;
  }
  return 1;
}

void pyfasttextureutils_rgb_to_hsv(int ri, int gi, int bi, int* hi, int* si, int* vi) {
  double r, g, b;
  double h, s, v;
  double min, max;
  double delta;
  
  r = (double)ri / 255;
  g = (double)gi / 255;
  b = (double)bi / 255;
  
  min = r;
  if (g < min)
    min = g;
  if (b < min)
    min = b;
  max = r;
  if (g > max)
    max = g;
  if (b > max)
    max = b;
  
  delta = (max - min);
  
  v = max;
  
  if (max > 0) {
    s = delta / max;
  } else {
    s = 0;
  }
  
  if (delta < 0.001) {
    // Black, white, or grey
    h = 0.0;
  } else {
    if (r >= max) {
      h = (g - b) / delta;
    } else if (g >= max) {
      h = (b - r) / delta + 2.0;
    } else {
      h = (r - g) / delta + 4.0;
    }
    
    h *= 60.0;
    
    if (h < 0) {
      h += 360.0;
    }
  }
  
  *hi = (int)h;
  *si = (int)(s*100);
  *vi = (int)(v*100);
}

void pyfasttextureutils_hsv_to_rgb(int hi, int si, int vi, int* ri, int* gi, int* bi) {
  double h, s, v;
  double r, g, b;
  double hue_int, hue_frac;
  double x, y, z;
  
  h = (double)(hi%360);
  s = (double)si / 100;
  v = (double)vi / 100;
  
  if (s < 0.001) {
    // Black, white, or grey
    r = g = b = v;
  } else {
    h /= 60.0;
    
    hue_int = floor(h);
    hue_frac = (h - hue_int);
    
    x = v * (1.0 - s);
    y = v * (1.0 - (s * hue_frac));
    z = v * (1.0 - (s * (1.0 - hue_frac)));
    
    switch ((int)hue_int) {
    case 0: r = v, g = z, b = x; break; // Red to yellow
    case 1: r = y, g = v, b = x; break; // Yellow to green
    case 2: r = x, g = v, b = z; break; // Green to cyan
    case 3: r = x, g = y, b = v; break; // Cyan to blue
    case 4: r = z, g = x, b = v; break; // Blue to purple
    case 5: r = v, g = x, b = y; break; // Purple to red
    }
  }
  
  *ri = (int)round(r*255);
  *gi = (int)round(g*255);
  *bi = (int)round(b*255);
}

static PyObject* pyfasttextureutils_color_exchange(PyObject* self, PyObject* args) {
  PyObject* src_image_bytes;
  PyObject* base_color;
  PyObject* replacement_color;
  PyObject* mask_image_bytes;
  int validate_mask_colors;
  int ignore_bright;
  
  if (!PyArg_ParseTuple(args, "SOOOpp", &src_image_bytes, &base_color, &replacement_color, &mask_image_bytes, &validate_mask_colors, &ignore_bright)) {
    return NULL; // Error already raised
  }
  
  unsigned char* src;
  src = PyBytes_AsString(src_image_bytes);
  if (!src) {
    return NULL; // Error already raised
  }
  
  int src_size = (int)PyBytes_Size(src_image_bytes);
  if (src_size % 4 != 0) {
    PyErr_SetString(PyExc_TypeError, "Input image data was not in RGBA mode.");
    return NULL;
  }
  
  unsigned char* mask;
  if (mask_image_bytes == Py_None) {
    mask = NULL;
  } else {
    mask = PyBytes_AsString(mask_image_bytes);
    if (!mask) {
      return NULL; // Error already raised
    }
    
    int mask_size = (int)PyBytes_Size(mask_image_bytes);
    if (mask_size != src_size) {
      PyErr_SetString(PyExc_TypeError, "Mask is not the same size as the texture.");
      return NULL;
    }
  }
  
  int base_r, base_g, base_b;
  int base_h, base_s, base_v;
  if (!pyfasttextureutils_color_tuple_to_rgb(base_color, &base_r, &base_g, &base_b)) {
    return NULL;
  }
  pyfasttextureutils_rgb_to_hsv(base_r, base_g, base_b, &base_h, &base_s, &base_v);
  
  int replacement_r, replacement_g, replacement_b;
  int replacement_h, replacement_s, replacement_v;
  if (!pyfasttextureutils_color_tuple_to_rgb(replacement_color, &replacement_r, &replacement_g, &replacement_b)) {
    return NULL;
  }
  pyfasttextureutils_rgb_to_hsv(replacement_r, replacement_g, replacement_b, &replacement_h, &replacement_s, &replacement_v);
  
  int s_change, v_change;
  s_change = replacement_s - base_s;
  v_change = replacement_v - base_v;
  
  unsigned char* dst;
  dst = malloc(src_size);
  if(!dst) {
    return PyErr_NoMemory();
  }
  
  for (int offset = 0; offset < src_size; offset += 4) {
    int pixel_is_masked = 1;
    
    if (mask) {
      int mask_r, mask_g, mask_b, mask_a;
      mask_r = mask[offset+0];
      mask_g = mask[offset+1];
      mask_b = mask[offset+2];
      mask_a = mask[offset+3];
      
      if (validate_mask_colors) {
        if (mask_r == 0xFF && mask_g == 0 && mask_b == 0 && mask_a == 0xFF) {
          // Red, masked
          pixel_is_masked = 1;
        } else if (mask_r == 0xFF && mask_g == 0xFF && mask_b == 0xFF && mask_a == 0xFF) {
          // White, unmasked
          pixel_is_masked = 0;
        } else if (mask_a == 0) {
          // Completely transparent, unmasked
          pixel_is_masked = 0;
        } else {
          // Any other color is invalid
          PyErr_SetString(PyExc_Exception, "Invalid color color in mask, only red (FF0000) and white (FFFFFF) should be present");
          return NULL;
        }
      } else {
        if (mask_r != 0xFF || mask_g != 0 || mask_b != 0 || mask_a != 0xFF) {
          // Not red, unmasked
          pixel_is_masked = 0;
        }
      }
    }
    
    int r, g, b, a;
    r = src[offset+0];
    g = src[offset+1];
    b = src[offset+2];
    a = src[offset+3];
    
    if (pixel_is_masked) {
      int h, s, v;
      
      pyfasttextureutils_rgb_to_hsv(r, g, b, &h, &s, &v);
      
      if (s == 0) {
        // Prevent issues when recoloring black/white/grey parts of a texture where the base color is not black/white/grey.
        s = base_s;
      }
      
      h = replacement_h;
      s += s_change;
      v += v_change;
      
      h %= 360;
      s = fmax(0, fmin(100, s));
      v = fmax(0, fmin(100, v));
      
      pyfasttextureutils_hsv_to_rgb(h, s, v, &r, &g, &b);
    }
    
    dst[offset+0] = r;
    dst[offset+1] = g;
    dst[offset+2] = b;
    dst[offset+3] = a;
  }
  
  int dst_size = src_size;
  PyObject* dst_bytes = PyBytes_FromStringAndSize(dst, dst_size);
  free(dst);
  return dst_bytes;
}

static PyMethodDef pyfasttextureutilsMethods[] = {
  {"color_exchange", pyfasttextureutils_color_exchange, METH_VARARGS, "Takes an image and replaces one color with another color while preserving hue and saturation differences between shades of that color. Optionally takes a mask for modifying only parts of the image."},
  {NULL, NULL, 0, NULL} // Sentinel
};

static struct PyModuleDef pyfasttextureutils_module = {
  PyModuleDef_HEAD_INIT,
  "pyfasttextureutils", // Module name
  NULL, // Documentation
  -1, // Size of per-interpreter state of the module, or -1 if the module keeps state in global variables.
  pyfasttextureutilsMethods
};

PyMODINIT_FUNC PyInit_pyfasttextureutils(void) {
  PyObject* module;
  
  module = PyModule_Create(&pyfasttextureutils_module);
  if (module == NULL) {
    return NULL;
  }
  
  return module;
}
