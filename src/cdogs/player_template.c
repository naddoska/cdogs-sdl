/*
	C-Dogs SDL
	A port of the legendary (and fun) action/arcade cdogs.

	Copyright (c) 2013-2014, 2016-2020, 2023 Cong Xu
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.
	Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include "player_template.h"

#include <json/json.h>

#include <cdogs/character.h>
#include <cdogs/files.h>
#include <cdogs/json_utils.h>
#include <cdogs/log.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define VERSION 4

PlayerTemplates gPlayerTemplates;

static void LoadPlayerTemplate(
	CArray *templates, json_t *node, const int version)
{
	PlayerTemplate t;
	memset(&t, 0, sizeof t);
	strncpy(
		t.name, json_find_first_label(node, "Name")->child->text,
		PLAYER_NAME_MAXLEN - 1);
	t.CharClassName = GetString(node, "Face");
	// Hair
	if (version < 3)
	{
		char *face;
		CSTRDUP(face, t.CharClassName);
		CharacterOldFaceToHeadParts(t.CharClassName, &t.CharClassName, t.HeadParts);
		CFREE(face);
	}
	else
	{
		LoadStr(&t.HeadParts[HEAD_PART_HAIR], node, "HairType");
		if (version < 4)
		{
			CharacterOldHairToHeadParts(t.HeadParts);
		}
		else
		{
			LoadStr(&t.HeadParts[HEAD_PART_FACEHAIR], node, "FacehairType");
			LoadStr(&t.HeadParts[HEAD_PART_HAT], node, "HatType");
			LoadStr(&t.HeadParts[HEAD_PART_GLASSES], node, "GlassesType");
		}
	}
	// Colors
	if (version == 1)
	{
		// Version 1 used integer palettes
		int skin, arms, body, legs, hair;
		LoadInt(&skin, node, "Skin");
		LoadInt(&arms, node, "Arms");
		LoadInt(&body, node, "Body");
		LoadInt(&legs, node, "Legs");
		LoadInt(&hair, node, "Hair");
		ConvertCharacterColors(skin, arms, body, legs, hair, &t.Colors);
	}
	else
	{
		LoadColor(&t.Colors.Skin, node, "Skin");
		LoadColor(&t.Colors.Arms, node, "Arms");
		LoadColor(&t.Colors.Body, node, "Body");
		LoadColor(&t.Colors.Legs, node, "Legs");
		LoadColor(&t.Colors.Hair, node, "Hair");
	}
	if (version < 3)
	{
		t.Colors.Feet = t.Colors.Legs;
	}
	LoadColor(&t.Colors.Feet, node, "Feet");
	if (version < 4)
	{
		t.Colors.Facehair = t.Colors.Hat = t.Colors.Glasses = t.Colors.Hair;
	}
	LoadColor(&t.Colors.Facehair, node, "Facehair");
	LoadColor(&t.Colors.Hat, node, "Hat");
	LoadColor(&t.Colors.Glasses, node, "Glasses");

	CArrayPushBack(templates, &t);
	LOG(LM_MAIN, LL_DEBUG, "loaded player template %s (%s)", t.name,
		t.CharClassName);
}

void PlayerTemplatesLoad(PlayerTemplates *pt, const CharacterClasses *classes)
{
	// Note: not used, but included in function to express dependency
	CASSERT(
		classes->Classes.size > 0,
		"cannot load player templates without character classes");
	json_t *root = NULL;

	CArrayInit(&pt->Classes, sizeof(PlayerTemplate));
	CArrayInit(&pt->CustomClasses, sizeof(PlayerTemplate));
	FILE *f = fopen(GetConfigFilePath(PLAYER_TEMPLATE_FILE), "r");
	if (!f)
	{
		LOG(LM_MAIN, LL_ERROR, "loading player templates '%s'",
			PLAYER_TEMPLATE_FILE);
		goto bail;
	}

	if (json_stream_parse(f, &root) != JSON_OK)
	{
		LOG(LM_MAIN, LL_ERROR, "parsing player templates '%s'",
			PLAYER_TEMPLATE_FILE);
		goto bail;
	}

	PlayerTemplatesLoadJSON(&pt->Classes, root);

bail:
	json_free_value(&root);
	if (f != NULL)
	{
		fclose(f);
	}
}

void PlayerTemplatesLoadJSON(CArray *classes, json_t *node)
{
	int version = 1;
	LoadInt(&version, node, "Version");

	if (json_find_first_label(node, "PlayerTemplates") == NULL)
	{
		LOG(LM_MAIN, LL_ERROR, "unknown player templates format");
		return;
	}
	json_t *child =
		json_find_first_label(node, "PlayerTemplates")->child->child;
	while (child != NULL)
	{
		LoadPlayerTemplate(classes, child, version);
		child = child->next;
	}
}

void PlayerTemplatesClear(CArray *classes)
{
	CA_FOREACH(PlayerTemplate, pt, *classes)
	CFREE(pt->CharClassName);
	for (HeadPart hp = HEAD_PART_HAIR; hp < HEAD_PART_COUNT; hp++)
	{
		CFREE(pt->HeadParts[hp]);
	}
	CA_FOREACH_END()
	CArrayClear(classes);
}

void PlayerTemplatesTerminate(PlayerTemplates *pt)
{
	PlayerTemplatesClear(&pt->Classes);
	CArrayTerminate(&pt->Classes);
	PlayerTemplatesClear(&pt->CustomClasses);
	CArrayTerminate(&pt->Classes);
}

PlayerTemplate *PlayerTemplateGetById(PlayerTemplates *pt, const int id)
{
	if (id < (int)pt->CustomClasses.size)
	{
		return CArrayGet(&pt->CustomClasses, id);
	}
	else if (id < (int)(pt->CustomClasses.size + pt->Classes.size))
	{
		return CArrayGet(&pt->Classes, id - (int)pt->CustomClasses.size);
	}
	return NULL;
}

static void SavePlayerTemplate(const PlayerTemplate *t, json_t *templates)
{
	json_t *node = json_new_object();
	AddStringPair(node, "Name", t->name);
	AddStringPair(node, "Face", t->CharClassName);
	if (t->HeadParts[HEAD_PART_HAIR])
	{
		AddStringPair(node, "HairType", t->HeadParts[HEAD_PART_HAIR]);
	}
	if (t->HeadParts[HEAD_PART_FACEHAIR])
	{
		AddStringPair(node, "FacehairType", t->HeadParts[HEAD_PART_FACEHAIR]);
	}
	if (t->HeadParts[HEAD_PART_HAT])
	{
		AddStringPair(node, "HatType", t->HeadParts[HEAD_PART_HAT]);
	}
	if (t->HeadParts[HEAD_PART_GLASSES])
	{
		AddStringPair(node, "GlassesType", t->HeadParts[HEAD_PART_GLASSES]);
	}
	AddColorPair(node, "Body", t->Colors.Body);
	AddColorPair(node, "Arms", t->Colors.Arms);
	AddColorPair(node, "Legs", t->Colors.Legs);
	AddColorPair(node, "Skin", t->Colors.Skin);
	AddColorPair(node, "Hair", t->Colors.Hair);
	AddColorPair(node, "Facehair", t->Colors.Facehair);
	AddColorPair(node, "Hat", t->Colors.Hat);
	AddColorPair(node, "Glasses", t->Colors.Glasses);
	AddColorPair(node, "Feet", t->Colors.Feet);
	json_insert_child(templates, node);
}
void PlayerTemplatesSave(const PlayerTemplates *pt)
{
	FILE *f = fopen(GetConfigFilePath(PLAYER_TEMPLATE_FILE), "w");
	char *text = NULL;
	char *formatText = NULL;
	json_t *root = json_new_object();

	if (f == NULL)
	{
		LOG(LM_MAIN, LL_ERROR, "saving player templates '%s'",
			PLAYER_TEMPLATE_FILE);
		goto bail;
	}

	json_insert_pair_into_object(
		root, "Version", json_new_number(TOSTRING(VERSION)));
	json_t *templatesNode = json_new_array();
	CA_FOREACH(const PlayerTemplate, t, pt->Classes)
	SavePlayerTemplate(t, templatesNode);
	CA_FOREACH_END()
	json_insert_pair_into_object(root, "PlayerTemplates", templatesNode);

	json_tree_to_string(root, &text);
	formatText = json_format_string(text);
	fputs(formatText, f);

bail:
	// clean up
	free(formatText);
	free(text);
	json_free_value(&root);
	if (f != NULL)
	{
		fclose(f);
#ifdef __EMSCRIPTEN__
		EM_ASM(
			// persist changes
			FS.syncfs(
				false, function(err) { assert(!err); }););
#endif
	}
}

void PlayerTemplateToPlayerData(PlayerData *p, const PlayerTemplate *t)
{
	memset(p->name, 0, sizeof p->name);
	strcpy(p->name, t->name);
	p->Char.Class = StrCharacterClass(t->CharClassName);
	if (p->Char.Class == NULL)
	{
		p->Char.Class = StrCharacterClass("Jones");
	}
	for (HeadPart hp = HEAD_PART_HAIR; hp < HEAD_PART_COUNT; hp++)
	{
		CFREE(p->Char.HeadParts[hp]);
		p->Char.HeadParts[hp] = NULL;
		if (t->HeadParts[hp])
		{
			CSTRDUP(p->Char.HeadParts[hp], t->HeadParts[hp]);
		}
	}
	p->Char.Colors = t->Colors;
}

static void PlayerTemplateFromCharacter(PlayerTemplate *t, const Character *c)
{
	CFREE(t->CharClassName);
	CSTRDUP(t->CharClassName, c->Class->Name);
	for (HeadPart hp = HEAD_PART_HAIR; hp < HEAD_PART_COUNT; hp++)
	{
		if (c->HeadParts[hp])
		{
			CFREE(t->HeadParts[hp]);
			CSTRDUP(t->HeadParts[hp], c->HeadParts[hp]);
		}
	}
	t->Colors = c->Colors;
}

void PlayerTemplateFromPlayerData(PlayerTemplate *t, const PlayerData *p)
{
	memset(t->name, 0, sizeof t->name);
	strcpy(t->name, p->name);
	PlayerTemplateFromCharacter(t, &p->Char);
}

void PlayerTemplateAddCharacter(CArray *classes, const Character *c)
{
	PlayerTemplate t;
	memset(&t, 0, sizeof t);
	strncpy(t.name, c->PlayerTemplateName, PLAYER_NAME_MAXLEN - 1);
	PlayerTemplateFromCharacter(&t, c);
	CArrayPushBack(classes, &t);
	LOG(LM_MAIN, LL_DEBUG, "loaded player template from characters %s (%s)",
		t.name, t.CharClassName);
}
