# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from webgl_conformance_expectations import WebGLConformanceExpectations

# See the GpuTestExpectations class for documentation.

class WebGL2ConformanceExpectations(WebGLConformanceExpectations):
  def __init__(self, conformance_path):
    super(WebGL2ConformanceExpectations, self).__init__(conformance_path)

  def SetExpectations(self):
    # All platforms.
    self.Skip('deqp/data/gles3/shaders/constant_expressions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/constants.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/conversions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/functions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/linkage.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/preprocessor.html', bug=483282)

    self.Skip('deqp/framework/opengl/simplereference/referencecontext.html',
        bug=483282)

    self.Skip('deqp/functional/gles3/attriblocation.html', bug=483282)
    self.Skip('deqp/functional/gles3/booleanstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/buffercopy.html', bug=483282)
    self.Skip('deqp/functional/gles3/builtinprecision*.html', bug=483282)
    self.Skip('deqp/functional/gles3/clipping.html', bug=483282)
    self.Skip('deqp/functional/gles3/draw.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbocolorbuffer.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbocompleteness.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbodepthbuffer.html', bug=483282)
    self.Skip('deqp/functional/gles3/fboinvalidate.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbomultisample.html', bug=483282)
    self.Skip('deqp/functional/gles3/fborender.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbostatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/fragdepth.html', bug=483282)
    self.Skip('deqp/functional/gles3/fragmentoutput.html', bug=483282)
    self.Skip('deqp/functional/gles3/framebufferblit.html', bug=483282)
    self.Skip('deqp/functional/gles3/indexedstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/instancedrendering.html', bug=483282)
    self.Skip('deqp/functional/gles3/integerstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/internalformatquery.html', bug=483282)
    self.Skip('deqp/functional/gles3/lifetime.html', bug=483282)
    self.Skip('deqp/functional/gles3/multisample.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativebufferapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativefragmentapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativeshaderapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativestateapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativetextureapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativevertexarrayapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/occlusionquery.html', bug=483282)
    self.Skip('deqp/functional/gles3/pixelbufferobject.html', bug=483282)
    self.Skip('deqp/functional/gles3/primitiverestart.html', bug=483282)
    self.Skip('deqp/functional/gles3/rasterizerdiscard.html', bug=483282)
    self.Skip('deqp/functional/gles3/samplerobject.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderbuiltinvar.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadercommonfunction.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderderivate.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderindexing.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderloop.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadermatrix.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderoperator.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderpackingfunction.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderprecision.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderstruct.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadertexturefunction*.html', bug=483282)
    self.Skip('deqp/functional/gles3/sync.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturefiltering*.html', bug=483282)
    self.Skip('deqp/functional/gles3/textureformat.html', bug=483282)
    self.Skip('deqp/functional/gles3/textureshadow.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturespecification*.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturewrap.html', bug=483282)
    self.Skip('deqp/functional/gles3/transformfeedback.html', bug=483282)
    self.Skip('deqp/functional/gles3/uniformapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/uniformbuffers.html', bug=483282)
    self.Skip('deqp/functional/gles3/vertexarrays.html', bug=483282)

    self.Fail('conformance2/buffers/uniform-buffers.html', bug=483282)
    self.Fail('conformance2/glsl3/array-complex-indexing.html', bug=483282)
    self.Fail('conformance2/glsl3/frag-depth.html', bug=483282)
    self.Fail('conformance2/glsl3/invalid-default-precision.html', bug=483282)
    self.Fail('conformance2/glsl3/sequence-operator-returns-non-constant.html',
        bug=483282)
    self.Fail('conformance2/glsl3/ternary-operator-on-arrays-glsl3.html',
        bug=483282)
    self.Fail('conformance2/reading/read-pixels-into-pixel-pack-buffer.html',
        bug=483282)
    self.Fail('conformance2/renderbuffers/framebuffer-test.html', bug=483282)
    self.Fail('conformance2/renderbuffers/invalidate-framebuffer.html',
        bug=483282)
    self.Fail('conformance2/samplers/sampler-drawing-test.html', bug=483282)
    self.Skip('conformance2/textures/webgl_canvas/*', bug=483282)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html', bug=483282)
    self.Fail('conformance2/textures/misc/tex-storage-2d.html', bug=483282)

    # Windows only.
    self.Skip('deqp/functional/gles3/readpixel.html', ['win'], bug=483282)
    self.Skip('deqp/functional/gles3/texturestatequery.html',
        ['win'], bug=483282)
    self.Fail('conformance2/glsl3/array-in-complex-expression.html',
        ['win'], bug=483282)
    self.Fail('conformance2/glsl3/short-circuiting-in-loop-condition.html',
        ['win'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['win'], bug=483282)
    self.Fail('conformance2/renderbuffers/framebuffer-object-attachment.html',
        ['win'], bug=1082) # angle bug ID
    self.Fail('conformance2/rendering/draw-buffers.html',
        ['win'], bug=483282)
    self.Fail('conformance2/state/gl-get-calls.html',
        ['win'], bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html',
        ['win'], bug=483282)
    self.Fail('conformance2/textures/canvas/*', ['win'], bug=483282)
    self.Fail('conformance2/textures/misc/gl-get-tex-parameter.html',
        ['win'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['win'], bug=483282)
    self.Skip('conformance2/transform_feedback/transform_feedback.html',
        ['win'], bug=483282)
    # Windows 8 only.
    self.Fail('conformance2/textures/image_data/tex-image-and-sub-image-2d' +
        '-with-image-data-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image_data/tex-image-and-sub-image-2d' +
        '-with-image-data-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image/tex-image-and-sub-image-2d' +
        '-with-image-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image/tex-image-and-sub-image-2d' +
        '-with-image-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/svg_image/tex-image-and-sub-image-2d' +
        '-with-svg-image-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/svg_image/tex-image-and-sub-image-2d' +
        '-with-svg-image-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/video/tex-image-and-sub-image-2d' +
        '-with-video-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/video/tex-image-and-sub-image-2d' +
        '-with-video-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)

    # Mac only.
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['mac'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/scoping.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/defaultvertexattribute.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/floatstatequery.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/texturestatequery.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/vertexarrayobject.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/shaderswitch.html',
        ['mavericks'], bug=483282)
    self.Fail('conformance2/attribs/gl-vertex-attrib-i-render.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/buffers/buffer-overflow-test.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/buffers/buffer-type-restrictions.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/renderbuffers/' +
        'multisampled-renderbuffer-initialization.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/rendering/instanced-arrays.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/samplers/samplers.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/canvas/*', ['mac'], bug=483282)
    self.Fail('conformance2/textures/video/*', ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/gl-get-tex-parameter.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-storage-compressed-formats.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/texture-npot.html',
        ['mac'], bug=483282)
    # Mac Intel only.
    self.Fail('deqp/data/gles3/shaders/arrays.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/conditionals.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/declarations.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/fragdata.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/invalid_texture_functions.html',
        ['mac', 'intel'], bug=536887)
    self.Fail('deqp/data/gles3/shaders/keywords.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/negative.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/switch.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/swizzles.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/bufferobjectquery.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/fbostencilbuffer.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/rbostatequery.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/readpixel.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/shaderapi.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/stringquery.html', ['mac', 'intel'],
        bug=536887)

    # Linux only.
    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/rendering/draw-buffers.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/samplers/samplers.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html',
        ['linux'], bug=483282)
    # Linux NVIDIA only.
    self.Skip('deqp/functional/gles3/shaderswitch.html',
        ['linux', 'nvidia'], bug=483282)
    # Linux AMD only (but fails on all Linux, so not specified as AMD specific)
    # It looks like AMD shader compiler rejects many valid ES3 semantics.
    self.Skip('deqp/data/gles3/shaders/arrays.html', ['linux'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/constant_expressions.html',
        ['linux'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['linux'], bug=483282)
