/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"

int			r_firstSceneDrawSurf;
#ifdef USE_PMLIGHT
int			r_firstSceneLitSurf;
#endif

int			r_numdlights;
int			r_firstSceneDlight;

int			r_numentities;
int			r_firstSceneEntity;

int			r_numpolys;
int			r_firstScenePoly;

int			r_numpolyverts;


/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame( void ) {

	backEndData->commands.used = 0;

	r_firstSceneDrawSurf = 0;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = 0;
#endif

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstScenePoly = r_numpolys;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	srfPoly_t	*poly;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, poly->fogIndex, 0 );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys ) {
	srfPoly_t	*poly;
	int			i, j;
	int			fogIndex;
	fog_t		*fog;
	vec3_t		bounds[2];

	if ( !tr.registered ) {
		return;
	}
#if 0
	if ( !hShader ) {
		ri.Printf( PRINT_WARNING, "RE_AddPolyToScene: NULL poly shader\n");
		return;
	}
#endif
	for ( j = 0; j < numPolys; j++ ) {
		if ( r_numpolyverts + numVerts > max_polyverts || r_numpolys >= max_polys ) {
      /*
      NOTE TTimo this was initially a PRINT_WARNING
      but it happens a lot with high fighting scenes and particles
      since we don't plan on changing the const and making for room for those effects
      simply cut this message to developer only
      */
			ri.Printf( PRINT_DEVELOPER, "WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n");
			return;
		}

		poly = &backEndData->polys[r_numpolys];
		poly->surfaceType = SF_POLY;
		poly->hShader = hShader;
		poly->numVerts = numVerts;
		poly->verts = &backEndData->polyVerts[r_numpolyverts];
		
		Com_Memcpy( poly->verts, &verts[numVerts*j], numVerts * sizeof( *verts ) );
#if 0
		if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
			poly->verts->modulate[0] = 255;
			poly->verts->modulate[1] = 255;
			poly->verts->modulate[2] = 255;
			poly->verts->modulate[3] = 255;
		}
#endif
		// done.
		r_numpolys++;
		r_numpolyverts += numVerts;

		// if no world is loaded
		if ( tr.world == NULL ) {
			fogIndex = 0;
		}
		// see if it is in a fog volume
		else if ( tr.world->numfogs == 1 ) {
			fogIndex = 0;
		} else {
			// find which fog volume the poly is in
			VectorCopy( poly->verts[0].xyz, bounds[0] );
			VectorCopy( poly->verts[0].xyz, bounds[1] );
			for ( i = 1 ; i < poly->numVerts ; i++ ) {
				AddPointToBounds( poly->verts[i].xyz, bounds[0], bounds[1] );
			}
			for ( fogIndex = 1 ; fogIndex < tr.world->numfogs ; fogIndex++ ) {
				fog = &tr.world->fogs[fogIndex]; 
				if ( bounds[1][0] >= fog->bounds[0][0]
					&& bounds[1][1] >= fog->bounds[0][1]
					&& bounds[1][2] >= fog->bounds[0][2]
					&& bounds[0][0] <= fog->bounds[1][0]
					&& bounds[0][1] <= fog->bounds[1][1]
					&& bounds[0][2] <= fog->bounds[1][2] ) {
					break;
				}
			}
			if ( fogIndex == tr.world->numfogs ) {
				fogIndex = 0;
			}
		}
		poly->fogIndex = fogIndex;
	}
}


//=================================================================================

static int isnan_fp( const float *f )
{
	uint32_t u = *( (uint32_t*) f );
	u = 0x7F800000 - ( u & 0x7FFFFFFF );
	return (int)( u >> 31 );
}


/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, qboolean intShaderTime ) {
	if ( !tr.registered ) {
		return;
	}
	if ( r_numentities >= MAX_REFENTITIES ) {
		ri.Printf( PRINT_DEVELOPER, "RE_AddRefEntityToScene: Dropping refEntity, reached MAX_REFENTITIES\n" );
		return;
	}
	if ( isnan_fp( &ent->origin[0] ) || isnan_fp( &ent->origin[1] ) || isnan_fp( &ent->origin[2] ) ) {
		static qboolean first_time = qtrue;
		if ( first_time ) {
			first_time = qfalse;
			ri.Printf( PRINT_WARNING, "RE_AddRefEntityToScene passed a refEntity which has an origin with a NaN component\n" );
		}
		return;
	}
	if ( (unsigned)ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData->entities[r_numentities].e = *ent;
	backEndData->entities[r_numentities].lightingCalculated = qfalse;
	backEndData->entities[r_numentities].intShaderTime = intShaderTime;

	r_numentities++;
}


/*
=====================
RE_AddDynamicLightToScene
=====================
*/
void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int additive ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	// these cards don't have the correct blend mode
	if ( glConfig.hardwareType == GLHW_RIVA128 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		return;
	}

	if (r_dlightDesaturate->value) {
		byte		bcol[3];

		bcol[0] = r * 0xff;
		bcol[1] = g * 0xff;
		bcol[2] = b * 0xff;

		if (r_dlightDesaturate->integer) {
			const float luma = LUMA(bcol[0], bcol[1], bcol[2]) / 255;
			r = g = b = luma;
		} else if (r_dlightDesaturate->value) {
			const float scale = fabs(r_dlightDesaturate->value);
			const float luma = LUMA(bcol[0], bcol[1], bcol[2]);

			r = LERP(bcol[0], luma, scale) / 255;
			g = LERP(bcol[1], luma, scale) / 255;
			b = LERP(bcol[2], luma, scale) / 255;
		}
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif
	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy( org, dl->origin );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = qfalse;
}


/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b  ) {
	dlight_t	*dl;
	if ( VectorCompare( start, end ) ) {
		RE_AddDynamicLightToScene( start, intensity, r, g, b, 0 );
		return;
	}
	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= ARRAY_LEN( backEndData->dlights ) ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer )
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif
	dl = &backEndData->dlights[ r_numdlights++ ];
	VectorCopy( start, dl->origin );
	VectorCopy( end, dl->origin2 );
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = 0;
	dl->linear = qtrue;
}



/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qfalse );
}


/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}


/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd, const qboolean cgame ) {
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}

	if ( r_noRefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	Com_Memcpy( tr.refdef.text, fd->text, sizeof( tr.refdef.text ) );

	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		for (i = 0 ; i < Q3_MAX_MAP_AREA_BYTES/4 ; i++) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}

//dm
	// do aspect correction for 3D HUD elements
	if ((tr.refdef.rdflags & RDF_NOWORLDMODEL) && cgame && arc_hud->integer && !backEnd.isHyperspace && !( backEnd.refdef.rdflags & RDF_HYPERSPACE ) ) {
		float x = tr.refdef.x, y = tr.refdef.y, w = tr.refdef.width, h = tr.refdef.height;

		RE_ScaleCorrection(&x, &y, &w, &h, -1);

		tr.refdef.x = (int)x;
		tr.refdef.y = (int)y;
		tr.refdef.width = (int)w;
		tr.refdef.height = (int)h;
	}
//-dm

	// derived info

	tr.refdef.floatTime = (double)tr.refdef.time * 0.001; // -EC-: cast to double

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData->drawSurfs;

#ifdef USE_PMLIGHT
	tr.refdef.numLitSurfs = r_firstSceneLitSurf;
	tr.refdef.litSurfs = backEndData->litSurfs;
#endif

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData->entities[r_firstSceneEntity];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData->dlights[r_firstSceneDlight];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData->polys[r_firstScenePoly];

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled
	if ( r_dynamicLight->integer == 0 || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;

	parms.scissorX = parms.viewportX;
	parms.scissorY = parms.viewportY;
	parms.scissorWidth = parms.viewportWidth;
	parms.scissorHeight = parms.viewportHeight;

	parms.portalView = PV_NONE;

#ifdef USE_PMLIGHT
	parms.dlights = tr.refdef.dlights;
	parms.num_dlights = tr.refdef.num_dlights;
#endif

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	//engine-based aspect correction, slightly modified code originally by leilei (openarena)
	// recalculate fov according to widescreen parameters
	if (arc_fov->integer && cgame && !(tr.refdef.rdflags & RDF_NOWORLDMODEL)) {
		const float afratio = (tr.refdef.fov_x / tr.refdef.fov_y);
		float zoomfov = tr.refdef.fov_x / 90;	// figure out our zoom or changed fov magnitiude from cg_fov and cg_zoomFOV

		// try not to recalculate fov of ui and hud elements
		if (afratio >= 1.3333) {
			const float aratio = ((float)glConfig.vidWidth / 640.0) - ((float)glConfig.vidHeight / 480.0);
			if (aratio > 1) {
				// undo vert-
				parms.fovY = parms.fovY * (73.739792 / tr.refdef.fov_y) * zoomfov;
			} else {
				// undo vert-
				parms.fovY = parms.fovY * (73.739792 / tr.refdef.fov_y) * zoomfov;
				// undo hor-
				parms.fovX = parms.fovX * (73.739792 / tr.refdef.fov_x) * zoomfov;
			}
		}
		// recalculate the fov
		parms.fovX = (atan(glConfig.vidWidth / (glConfig.vidHeight / tan((parms.fovY * M_PI) / 360.0f))) * 360.0f) / M_PI;
		parms.fovY = (atan(glConfig.vidHeight / (glConfig.vidWidth / tan((parms.fovX * M_PI) / 360.0f))) * 360.0f) / M_PI;
	}

	parms.stereoFrame = tr.refdef.stereoFrame;

	VectorCopy( fd->vieworg, parms.or.origin );
	VectorCopy( fd->viewaxis[0], parms.or.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.or.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.or.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

	R_RenderView( &parms );

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
#ifdef USE_PMLIGHT
	r_firstSceneLitSurf = tr.refdef.numLitSurfs;
#endif

	r_firstSceneEntity = r_numentities;
	r_firstSceneDlight = r_numdlights;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
