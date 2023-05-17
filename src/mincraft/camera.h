#pragma once

#include "glm_include.h"

namespace utils3d
{

struct Camera final
{
    Camera(
        glm::vec3 p_pos = { 0, 0, 0 },
        glm::vec3 p_dir = { 0, 0, -1.0f },
        glm::vec3 p_up = { 0, 1.0f, 0 },
        float p_fov = glm::radians( 45.0 ),
        float p_near_clip = 0.1f,
        float p_far_clip = 1000.0f )
        : m_direction{ glm::normalize( p_dir ) },
          m_up{ glm::normalize( p_up ) },
          m_fov{ p_fov },
          m_z_near_clip{ p_near_clip },
          m_z_far_clip{ p_far_clip },
          position{ p_pos }
    {
    }

  public:
    void setFovDegrees( float degrees ) { m_fov = glm::radians( degrees ); }
    float getFovDegrees() const { return glm::degrees( m_fov ); }

    void translate( glm::vec3 translation ) { position += translation; }
    glm::vec3 getDir() const { return m_direction; }
    glm::vec3 getUp() const { return m_up; }
    glm::vec3 getSideways() const { return glm::cross( m_direction, m_up ); }

    void setNearClip( float near )
    {
        assert( near > 0.0f && near < m_z_far_clip );
        m_z_near_clip = near;
    }

    void setFarClip( float far )
    {
        assert( far > 0.0f && m_z_near_clip < far );
        m_z_far_clip = far;
    }

    void rotate( glm::quat q )
    {
        m_direction = m_direction * q;
        m_up = m_up * q;
    }

  public:
    struct Matrices
    {
        glm::mat4x4 view;
        glm::mat4x4 proj;
    };

    Matrices getMatrices( uint32_t width, uint32_t height )
    {
        auto view = glm::lookAt( position, position + m_direction, m_up );
        auto proj = glm::perspective( m_fov, static_cast<float>( width ) / height, m_z_near_clip, m_z_far_clip );
        return Matrices{ .view = view, .proj = proj };
    }

  private:
    glm::vec3 m_direction, m_up;
    float m_fov, m_z_near_clip, m_z_far_clip;

  public:
    glm::vec3 position;
};

} // namespace utils3d