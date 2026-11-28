#include "render/gpu_timer.hpp"

#include <cstddef>
#include <vector>

#include <glad/gl.h>

namespace manifold {

GpuTimer::GpuTimer() { glGenQueries(static_cast<GLsizei>(kQueries), queries_.data()); }

GpuTimer::~GpuTimer() { glDeleteQueries(static_cast<GLsizei>(kQueries), queries_.data()); }

void GpuTimer::begin() {
    // A query whose result hasn't come in yet can't be started again: this frame isn't measured.
    if (pending_[next_]) {
        return;
    }
    glBeginQuery(GL_TIME_ELAPSED, queries_[next_]);
    measuring_ = true;
}

void GpuTimer::end() {
    if (!measuring_) {
        return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    measuring_ = false;
    pending_[next_] = true;
    next_ = (next_ + 1) % kQueries;
}

std::vector<float> GpuTimer::collect() {
    std::vector<float> seconds;
    // The oldest query comes first: the one at next_ was ended longest ago.
    for (std::size_t k = 0; k < kQueries; ++k) {
        const std::size_t i = (next_ + k) % kQueries;
        if (!pending_[i]) {
            continue;
        }
        GLint available = GL_FALSE;
        glGetQueryObjectiv(queries_[i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_FALSE) {
            break;  // Later queries finish later.
        }
        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(queries_[i], GL_QUERY_RESULT, &nanoseconds);
        pending_[i] = false;
        seconds.push_back(static_cast<float>(nanoseconds) * 1e-9f);
    }
    return seconds;
}

}  // namespace manifold
