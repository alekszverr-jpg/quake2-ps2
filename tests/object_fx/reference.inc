void CL_BlasterTrail_Reference(vec3_t start, vec3_t end)
{
    vec3_t move;
    vec3_t vec;
    float len;
    int j;
    cparticle_t * p;
    int dec;

    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    dec = 5;
    VectorScale(vec, 5, vec);

    // FIXME: this is a really silly way to have a loop
    while (len > 0)
    {
        len -= dec;

        if (!free_particles)
            return;
        p = free_particles;
        free_particles = p->next;
        p->next = active_particles;
        active_particles = p;
        VectorClear(p->accel);

        p->time = cl.time;

        p->alpha = 1.0;
        p->alphavel = -1.0 / (0.3 + frand() * 0.2);
        p->color = 0xe0;
        for (j = 0; j < 3; j++)
        {
            p->org[j] = move[j] + crand();
            p->vel[j] = crand() * 5;
            p->accel[j] = 0;
        }

        VectorAdd(move, vec, move);
    }
}
void CL_DiminishingTrail_Reference(vec3_t start, vec3_t end, centity_t * old, int flags)
{
    vec3_t move;
    vec3_t vec;
    float len;
    int j;
    cparticle_t * p;
    float dec;
    float orgscale;
    float velscale;

    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    dec = 0.5;
    VectorScale(vec, dec, vec);

    if (old->trailcount > 900)
    {
        orgscale = 4;
        velscale = 15;
    }
    else if (old->trailcount > 800)
    {
        orgscale = 2;
        velscale = 10;
    }
    else
    {
        orgscale = 1;
        velscale = 5;
    }

    while (len > 0)
    {
        len -= dec;

        if (!free_particles)
            return;

        // drop less particles as it flies
        if ((rand() & 1023) < old->trailcount)
        {
            p = free_particles;
            free_particles = p->next;
            p->next = active_particles;
            active_particles = p;
            VectorClear(p->accel);

            p->time = cl.time;

            if (flags & EF_GIB)
            {
                p->alpha = 1.0;
                p->alphavel = -1.0 / (1 + frand() * 0.4);
                p->color = 0xe8 + (rand() & 7);
                for (j = 0; j < 3; j++)
                {
                    p->org[j] = move[j] + crand() * orgscale;
                    p->vel[j] = crand() * velscale;
                    p->accel[j] = 0;
                }
                p->vel[2] -= PARTICLE_GRAVITY;
            }
            else if (flags & EF_GREENGIB)
            {
                p->alpha = 1.0;
                p->alphavel = -1.0 / (1 + frand() * 0.4);
                p->color = 0xdb + (rand() & 7);
                for (j = 0; j < 3; j++)
                {
                    p->org[j] = move[j] + crand() * orgscale;
                    p->vel[j] = crand() * velscale;
                    p->accel[j] = 0;
                }
                p->vel[2] -= PARTICLE_GRAVITY;
            }
            else
            {
                p->alpha = 1.0;
                p->alphavel = -1.0 / (1 + frand() * 0.2);
                p->color = 4 + (rand() & 7);
                for (j = 0; j < 3; j++)
                {
                    p->org[j] = move[j] + crand() * orgscale;
                    p->vel[j] = crand() * velscale;
                }
                p->accel[2] = 20;
            }
        }

        old->trailcount -= 5;
        if (old->trailcount < 100)
            old->trailcount = 100;
        VectorAdd(move, vec, move);
    }
}
void CL_RocketTrail_Reference(vec3_t start, vec3_t end, centity_t * old)
{
    vec3_t move;
    vec3_t vec;
    float len;
    int j;
    cparticle_t * p;
    float dec;

    // smoke
    CL_DiminishingTrail_Reference(start, end, old, EF_ROCKET);

    // fire
    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    dec = 1;
    VectorScale(vec, dec, vec);

    while (len > 0)
    {
        len -= dec;

        if (!free_particles)
            return;

        if ((rand() & 7) == 0)
        {
            p = free_particles;
            free_particles = p->next;
            p->next = active_particles;
            active_particles = p;

            VectorClear(p->accel);
            p->time = cl.time;

            p->alpha = 1.0;
            p->alphavel = -1.0 / (1 + frand() * 0.2);
            p->color = 0xdc + (rand() & 3);
            for (j = 0; j < 3; j++)
            {
                p->org[j] = move[j] + crand() * 5;
                p->vel[j] = crand() * 20;
            }
            p->accel[2] = -PARTICLE_GRAVITY;
        }
        VectorAdd(move, vec, move);
    }
}