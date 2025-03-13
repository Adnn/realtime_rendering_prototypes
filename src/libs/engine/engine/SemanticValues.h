#pragma once


namespace ad::renderer {


namespace semantic
{


    #define SEM(s) const Semantic g ## s{#s}

    // TODO Ad: Reconsider wether we want to ever capture the "builtin" semantic
    // Currently used as a special case for program introspection.
    SEM(_builtin);

    SEM(Bitangent);
    SEM(Position);
    SEM(Tangent);
    SEM(Uv01);

    #undef SEM


    #define BLOCK_SEM(s) const BlockSemantic g ## s{#s}

    #undef BLOCK_SEM


} // namespace semantic


} // namespace ad::renderer