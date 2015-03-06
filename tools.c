#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#include "uthash.h"
#include "citrine.h"

obj* World = NULL;
obj* contexts[100];
int cid = 0;
obj* ctr_build_number(char* number);
obj* Object;
obj* CBlock;
obj* TextString;
obj* Number;
obj* BoolX;
obj* Console;
obj* Nil;
obj* GC;
int debug;

//measures the size of character
int utf8size(char c) {
	if ((c & UTF8_BYTE3) == UTF8_BYTE3) return 4;
	if ((c & UTF8_BYTE2) == UTF8_BYTE2) return 3;
	if ((c & UTF8_BYTE1) == UTF8_BYTE1) return 2;
	return 1;
}
//measures the length of an utf8 string in utf8 chars
long getutf8len(char* strval, long max) {
	long i;
	long j = 0;
	int s = 0;
	for(i = 0; i < max; i++) {
		s = utf8size(strval[i]);
		j += (s - 1);
	}
	return (i-j);
}

long getBytesUtf8(char* strval, long startByte, long lenUChar) {
	long i = 0;
	long bytes = 0;
	int s = 0;
	int x = 0;
	long index = 0;
	while(x < lenUChar) {
		index = startByte + i;
		char c = strval[index];
		s = utf8size(c);
		bytes = bytes + s;
		i = i + s;
		x ++;
	}
	return bytes;
}

char* readf(char* file_name) {
   char* prg;
   char ch;
   FILE* fp;
   fp = fopen(file_name,"r");
   if( fp == NULL )
   {
      printf("Error while opening the file.\n");
      exit(1);
   }
   int prev = ftell(fp);
   fseek(fp,0L,SEEK_END);
   int sz = ftell(fp);
   fseek(fp,prev,SEEK_SET);
   prg = malloc((sz+1)*sizeof(char));
   int i=0;
   while( ( ch = fgetc(fp) ) != EOF )
     prg[i++]=ch;
   fclose(fp);
   return prg;
}

void tree(tnode* ti, int indent) {
	if (indent>20) exit(1); 
	tlistitem* li = ti->nodes;
	tnode* t = li->node;
	while(1) {
		int i = 0;
		for (i=0; i<indent; i++) printf(" ");
		char* str = calloc(40, sizeof(char));
		if (t->type == EXPRASSIGNMENT) 		str = "ASSIGN\0";
		else if (t->type == EXPRMESSAGE) 	str = "MESSAG\0";
		else if (t->type == UNAMESSAGE) 	str = "UMSSAG\0";
		else if (t->type == KWMESSAGE) 		str = "KMSSAG\0";
		else if (t->type == BINMESSAGE) 	str = "BMSSAG\0";
		else if (t->type == LTRSTRING) 		str = "STRING\0";
		else if (t->type == REFERENCE) 		str = "REFRNC\0";
		else if (t->type == LTRNUM) 		str = "NUMBER\0";
		else if (t->type == CODEBLOCK) 		str = "CODEBL\0";
		else if (t->type == RETURNFROMBLOCK)str = "RETURN\0";
		else if (t->type == PARAMLIST)		str = "PARAMS\0";
		else if (t->type == INSTRLIST)		str = "INSTRS\0";
		else if (t->type == ENDOFPROGRAM)	str = "EOPROG\0";
		else if (t->type == NESTED)	        str = "NESTED\0";
		else if (t->type == LTRBOOLFALSE)	str = "BFALSE\0";
		else if (t->type == LTRBOOLTRUE)	str = "BLTRUE\0";
		else if (t->type == LTRNIL)	        str = "LTRNIL\0";
		else 								str = "UNKNW?\0";
		printf("%d:%s %s (vlen: %lu) \n", t->type, str, t->value, t->vlen);
		if (t->nodes) tree(t, indent + 1);
		if (!li->next) break; 
		li = li->next;
		t = li->node;
	}
}

void ctr_open_context() {
	cid++;
	obj* context = NULL;
	context = calloc(1, sizeof(obj));
	context->name = "Context";
	context->value = NULL;
	contexts[cid] = context;
}

void ctr_close_context() {
	if (cid == 0) return;
	cid--;
}

obj* ctr_find(char* key) {
	int i = cid;
	obj* foundObject = NULL;
	foundObject = calloc(sizeof(obj), 1);
	int first = 1;
	while((i>-1 && foundObject == NULL) || first) {
		first = 0;
		obj* context = contexts[i];
		HASH_FIND_STR(context->properties, key, foundObject);
		i--;
	}
	if (foundObject == NULL) { printf("Error, key not found: %s.\n", key); exit(1); }
	return foundObject;
}

obj* ctr_find_in_my(char* key) {
	obj* foundObject = CTR_CREATE_OBJECT();
	obj* context = ctr_find("me");
	HASH_FIND_STR(context->link->properties, key, foundObject);
	if (foundObject == NULL) { printf("Error, property not found: %s.\n", key); exit(1); }
	return foundObject;
}

void ctr_set(obj* object) {
	obj* foundObject = CTR_CREATE_OBJECT();
	obj* context = contexts[cid];
	HASH_FIND_STR(context->properties, object->name, foundObject);
	if (foundObject) {
		HASH_DELETE(hh, context->properties, foundObject);
	}
	HASH_ADD_KEYPTR(hh, context->properties, object->name, strlen(object->name), object);
}

obj* ctr_build_bool(int truth) {
	obj* boolObject = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(boolObject);
	ASSIGN_STRING(boolObject,name,"Bool",4);
	if (truth) boolObject->value = "1"; else boolObject->value = "0";
	boolObject->vlen = 1;
	boolObject->type = OTBOOL;
	boolObject->link = BoolX;
	return boolObject;
}


obj* ctr_console_write(obj* myself, args* argumentList) {
	obj* argument1 = argumentList->object;
	printf("%s", argument1->value);
	return myself;
}

obj* ctr_block_run(obj* myself, args* argList, obj* my) {
	obj* selfRef = CTR_CREATE_OBJECT();
	selfRef->name = "me";
	selfRef->type = OTOBJECT;
	ASSIGN_STRING(selfRef,value,"[self]",6);
	selfRef->link = my;
	obj* result;
	tnode* node = myself->block;
	obj* thisBlock = CTR_CREATE_OBJECT();
	ASSIGN_STRING(thisBlock,name,"__currentblock__",16);
	thisBlock->type = OTBLOCK;
	ASSIGN_STRING(thisBlock,value,"[running block]",15);
	thisBlock->link = myself;
	tlistitem* codeBlockParts = node->nodes;
	tnode* codeBlockPart1 = codeBlockParts->node;
	tnode* codeBlockPart2 = codeBlockParts->next->node;
	tlistitem* parameterList = codeBlockPart1->nodes;
	tnode* parameter;
	if (parameterList && parameterList->node) {
		parameter = parameterList->node;
		obj* a;
		while(1) {
			if (parameter && argList->object) {
				a = argList->object;
				a->name = parameter->value;
				ctr_set(a);
			}
			if (!argList->next) break;
			argList = argList->next;
			if (!parameterList->next) break;
			parameterList = parameterList->next;
			parameter = parameterList->node;
		}
	}
	ctr_open_context();
	ctr_set(selfRef);
	ctr_set(thisBlock); //otherwise running block may get gc'ed.
	result = cwlk_run(codeBlockPart2);
	ctr_close_context();
	return result;
}

obj* ctr_block_runIt(obj* myself, args* argumentList) {
	return ctr_block_run(myself, argumentList, myself);
}

obj* ctr_build_block(tnode* node) {
	obj* codeBlockObject = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(codeBlockObject);
	codeBlockObject->type = OTBLOCK;
	codeBlockObject->block = node;
	codeBlockObject->link = CBlock;
	ASSIGN_STRING(codeBlockObject,value,"[block]",7);
	return codeBlockObject;
}

obj* ctr_bool_iftrue(obj* myself, args* argumentList) {
	if (strncmp(myself->value,"1",1)==0) {
		obj* codeBlock = argumentList->object;
		args* arguments = CTR_CREATE_ARGUMENT();
		arguments->object = myself;
		return ctr_block_run(codeBlock, arguments, myself);
	}
	return myself;
}

obj* ctr_bool_ifFalse(obj* myself, args* argumentList) {
	if (strncmp(myself->value,"0",1)==0) {
		obj* codeBlock = argumentList->object;
		args* arguments = CTR_CREATE_ARGUMENT();
		arguments->object = myself;
		return ctr_block_run(codeBlock, arguments, myself);
	}
	return myself;
}

obj* ctr_bool_opposite(obj* myself, args* argumentList) {
	if (strncmp(myself->value,"1",1)==0) myself->value = "0"; else myself->value = "1";
	return myself;
}

obj* ctr_bool_and(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (argumentList->object->type != OTBOOL) {
		printf("Argument of binary message && must be a boolean.\n"); exit(1);
	}
	return ctr_build_bool(( (strncmp(myself->value,"1",1)==0) && (strncmp(argumentList->object->value,"1",1)==0)  ));
}

obj* ctr_bool_or(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (argumentList->object->type != OTBOOL) {
		printf("Argument of binary message || must be a boolean.\n"); exit(1);
	}
	return ctr_build_bool(( (strncmp(myself->value,"1",1)==0) || (strncmp(argumentList->object->value,"1",1)==0)  ));
}

obj* ctr_number_higherThan(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a > b));
	return truth;
}

obj* ctr_number_higherEqThan(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a >= b));
	return truth;
}

obj* ctr_number_lowerThan(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a < b));
	return truth;
}

obj* ctr_number_lowerEqThan(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a <= b));
	return truth;
}

obj* ctr_number_eq(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a == b));
	return truth;
}

obj* ctr_number_neq(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	obj* truth = ctr_build_bool((a != b));
	return truth;
}

obj* ctr_number_between(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	if (!argumentList->next) { printf("Expected second number."); exit(1); }
	args* nextArgumentItem = argumentList->next;
	obj* nextArgument = nextArgumentItem->object;
	if (nextArgument->type != OTNUMBER) { printf("Expected second argument to be number."); exit(1); }
	float c = atof(nextArgument->value);
	obj* truth = ctr_build_bool((a >= b) && (a <= c));
	return truth;
}

obj* ctr_number_add(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a+b));
	obj* newNum = ctr_build_number(str);
	return newNum;
}

obj* ctr_number_inc(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a+b));
	long len = strlen(str);
	ASSIGN_STRING(myself,value,str,len);
	myself->vlen = len;
	return myself;
}

obj* ctr_number_minus(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a-b));
	obj* newNum = ctr_build_number(str);
	return newNum;
}

obj* ctr_number_dec(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a-b));
	long len = strlen(str);
	ASSIGN_STRING(myself,value,str,len);
	myself->vlen = len;
	return myself;
}

obj* ctr_number_multiply(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a*b));
	obj* newNum = ctr_build_number(str);
	return newNum;
}

obj* ctr_number_mul(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a*b));
	long len = strlen(str);
	ASSIGN_STRING(myself,value,str,len);
	myself->vlen = len;
	return myself;
}


obj* ctr_number_divide(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	if (b == 0) {
		printf("Division by zero.");
		exit(1);
	}
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a/b));
	obj* newNum = ctr_build_number(str);
	return newNum;
}

obj* ctr_number_div(obj* myself, args* argumentList) {
	obj* otherNum = argumentList->object;
	if (otherNum->type != OTNUMBER) { printf("Expected number."); exit(1); }
	float a = atof(myself->value);
	float b = atof(otherNum->value);
	if (b == 0) {
		printf("Division by zero.");
		exit(1);
	}
	char* str = calloc(sizeof(char), 40);
	sprintf(str, "%f", (a/b));
	long len = strlen(str);
	ASSIGN_STRING(myself,value,str,len);
	myself->vlen = len;
	return myself;
}


obj* ctr_number_factorial(obj* myself, args* argumentList) {
	float t = floor(atof(myself->value));
	int i;
	float a = 1;
	for(i = (int) t; i > 0; i--) {
		a = a * i;
	}
	char* str = calloc(40, sizeof(char));
	sprintf(str, "%f", (a));
	ASSIGN_STRING(myself,value,str,strlen(str));
	return myself;
}

obj* ctr_number_times(obj* myself, args* argumentList) {
	obj* block = argumentList->object;
	if (block->type != OTBLOCK) { printf("Expected code block."); exit(1); }
	block->mark = 2; //mark as sticky
	int t = atoi(myself->value);
	int i;
	for(i=0; i<t; i++) {
		char* nstr = (char*) calloc(20, sizeof(char));
		snprintf(nstr, 20, "%d", i);
		obj* indexNumber = ctr_build_number(nstr);
		args* arguments = CTR_CREATE_ARGUMENT();
		arguments->object = indexNumber;
		ctr_block_run(block, arguments, myself);
	}
	block->mark = 0;
	return myself;
}

//create number from \0 terminated string
obj* ctr_build_number(char* n) {
	int s = strlen(n);
	obj* numberObject = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(numberObject);
	ASSIGN_STRING(numberObject,name,"Number",6);
	ASSIGN_STRING(numberObject,value,n,s);
	numberObject->vlen = s;	
	numberObject->type = OTNUMBER;
	numberObject->link = Number;
	return numberObject;
}

obj* ctr_object_make() {
	obj* objectInstance = NULL;
	objectInstance = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(objectInstance);
	objectInstance->type = OTOBJECT;
	ASSIGN_STRING(objectInstance,value,"[object]", 8);
	objectInstance->link = Object;
	return objectInstance;
}

obj* ctr_object_method_does(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (!argumentList->next) {
		printf("Missing argument 2\n"); exit(1);
	}
	obj* methodName = argumentList->object;
	if (methodName->type != OTSTRING) {
		printf("Expected argument method: to be of type string.\n");
		exit(1);
	}
	args* nextArgument = argumentList->next;
	if (!nextArgument->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	obj* methodBlock = nextArgument->object;
	if (methodBlock->type != OTBLOCK) {
		printf("Expected argument does: to be of type block.\n");
		exit(1);
	}
	methodBlock->name = methodName->value;
	if (debug) printf("Adding method block: %s %s to %s.\n", methodBlock->value, methodBlock->name, myself->name);
	HASH_ADD_KEYPTR(hh, myself->methods, methodBlock->name, strlen(methodBlock->name), methodBlock);
	return myself;
}


obj* ctr_object_override_does(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (!argumentList->next) {
		printf("Missing argument 2\n"); exit(1);
	}
	obj* methodName = argumentList->object;
	if (methodName->type != OTSTRING) {
		printf("Expected argument method: to be of type string.\n");
		exit(1);
	}
	args* nextArgument = argumentList->next;
	if (!nextArgument->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	obj* methodBlock = nextArgument->object;
	if (methodBlock->type != OTBLOCK) {
		printf("Expected argument does: to be of type block.\n");
		exit(1);
	}
	methodBlock->name = methodName->value;
	if (debug) printf("Adding method block: %s %s to %s.\n", methodBlock->value, methodBlock->name, myself->name);
	obj* oldBlock = CTR_CREATE_OBJECT();
	HASH_FIND_STR(myself->methods, methodBlock->name, oldBlock);
	if (!oldBlock) printf("Cannot override: %s no such method.", oldBlock->name);
	char* str = (char*) calloc(255, sizeof(char));
	strcat(str, "overridden-");
	strcat(str, oldBlock->name);
	oldBlock->name = str;
	HASH_DEL(myself->methods, oldBlock);
	HASH_ADD_KEYPTR(hh, myself->methods, oldBlock->name, strlen(oldBlock->name), oldBlock);
	HASH_ADD_KEYPTR(hh, myself->methods, methodBlock->name, strlen(methodBlock->name), methodBlock);
	return myself;
}


obj* ctr_object_blueprint(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	obj* other = argumentList->object;
	if (other->type != OTOBJECT) {
		printf("Expected argument method: to be of type object.\n");
		exit(1);
	}
	myself->link = other;
	return myself;
}

obj* ctr_build_string(char* stringValue, long size) {
	obj* stringObject = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(stringObject);
	ASSIGN_STRING(stringObject,name,"String",6);
	ASSIGN_STRING(stringObject,value,stringValue,size);
	stringObject->vlen = size;
	stringObject->type = OTSTRING;
	stringObject->link = TextString;
	return stringObject;
}

obj* ctr_string_bytes(obj* myself, args* argumentList) {
	char* str = calloc(100, sizeof(char));
	long l = (myself->vlen);
	sprintf(str, "%lu", l);
	return ctr_build_number(str);
}

obj* ctr_string_eq(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (argumentList->object->vlen != myself->vlen) {
		return ctr_build_bool(0);
	}
	return ctr_build_bool((strncmp(argumentList->object->value, myself->value, myself->vlen)==0));
}

obj* ctr_string_length(obj* myself, args* argumentList) {
	long n = getutf8len(myself->value, myself->vlen);
	char* str = calloc(100, sizeof(char));
	sprintf(str, "%lu", n);
	return ctr_build_number(str);
}

obj* ctr_string_printbytes(obj* myself, args* argumentList) {
	char* str = myself->value;
	long n = myself->vlen;
	long i = 0;
	for(i = 0; i < n; i++) printf("%u ", (unsigned char) str[i]);
	printf("\n");
	return myself;
}

obj* ctr_string_concat(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	long n1 = myself->vlen;
	long n2 = argumentList->object->vlen;
	char* dest = calloc(sizeof(char), (n1 + n2));
	strncpy(dest, myself->value, n1);
	strncat(dest, argumentList->object->value, n2);
	obj* newString = ctr_build_string(dest, (n1 + n2));
	return newString;	
}

obj* ctr_string_fromto(obj* myself, args* argumentList) {
	if (!argumentList->object) {
		printf("Missing argument 1\n"); exit(1);
	}
	if (!argumentList->next) {
		printf("Missing argument 2\n"); exit(1);
	}
	obj* fromPos = argumentList->object;
	obj* toPos = argumentList->next->object;
	
	long a = atol(fromPos->value);
	long b = atol(toPos->value); 
	long ua = getBytesUtf8(myself->value, 0, a);
	long ub = getBytesUtf8(myself->value, ua, ((b - a) + 1));
	char* dest = calloc(ub, sizeof(char));
	strncpy(dest, (myself->value) + ua, ub);
	obj* newString = ctr_build_string(dest,ub);
	return newString;
}

obj* ctr_build_nil() {	
	return Nil;
}

obj* ctr_nil_isnil(obj* myself, args* argumentList) {
	obj* truth;
	if ((myself->type == OTNIL)) {
		truth = ctr_build_bool(1);
	} else {
		truth = ctr_build_bool(0);
	}
	return truth;
}

void ctr_gc_mark(obj* object) {
	obj* item;
	obj* tmp;
	HASH_ITER(hh, object->properties, item, tmp) {
		if (item->mark == 2) continue;
		item->mark = 1;
		ctr_gc_mark(item);
	}
	HASH_ITER(hh, object->methods, item, tmp) {
		if (item->mark == 2) continue;
		item->mark = 1;
		ctr_gc_mark(item);
	}
}

void ctr_gc_sweep() {
	obj** q = &ctr_first_object;
	while(*q) {
		if ((*q)->mark==0){
			obj* u = *q;
			*q = u->next;
			int z;
			for(z =0; z <= cid; z++) {
				HASH_DELETE(hh, contexts[z], u);
			}
			free(u);
		} else {
			if ((*q)->mark == 1) (*q)->mark = 0;
			q = &(*q)->next;
		}
	}
}

void ctr_gc_collect (obj* myself, args* argumentList) {
	obj* context = contexts[cid];
	int oldcid = cid;
	while(cid > -1) {
		ctr_gc_mark(context);
		cid --;
		context = contexts[cid];
	}
	ctr_gc_sweep();
	cid = oldcid;
}

void ctr_initialize_world() {
	
	CTR_INIT_HEAD_OBJECT();
	
	CTR_CREATE_OBJECT_TYPE(World, "World", "[world]", 7, OTOBJECT);
	World->mark = 2;
	contexts[0] = World;
	
	CTR_CREATE_OBJECT_TYPE(Console, "Console", "[console]", 9, OTOBJECT)
	CTR_CREATE_FUNC(ConsoleWrite, &ctr_console_write, "write:", Console);
	Console->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(GC, "GC", "[GC]", 4, OTOBJECT)
	CTR_CREATE_FUNC(GCCollect, &ctr_gc_collect, "collect", GC);
	GC->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(Object, "Object", "[object]",8, OTOBJECT);
	CTR_CREATE_FUNC(ObjectMake, &ctr_object_make, "new", Object);
	CTR_CREATE_FUNC(ObjectMethodDoes, &ctr_object_method_does, "method:does:", Object);
	CTR_CREATE_FUNC(ObjectOverrideDoes, &ctr_object_override_does, "override:does:", Object);
	CTR_CREATE_FUNC(ObjectBlueprint, &ctr_object_blueprint, "basedOn:", Object);
	Object->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(Number, "Number", "0", 1, OTNUMBER);
	CTR_CREATE_FUNC(numberTimesObject, &ctr_number_times, "times:", Number);
	CTR_CREATE_FUNC(numberAdd, &ctr_number_add, "+", Number);
	CTR_CREATE_FUNC(numberInc, &ctr_number_inc, "inc:", Number);
	CTR_CREATE_FUNC(numberMin, &ctr_number_minus, "-", Number);
	CTR_CREATE_FUNC(numberDec, &ctr_number_dec, "dec:", Number);
	CTR_CREATE_FUNC(numberMul, &ctr_number_multiply, "*", Number);
	CTR_CREATE_FUNC(numberMuls, &ctr_number_mul, "mul:", Number);
	CTR_CREATE_FUNC(numberDiv, &ctr_number_divide, "/", Number);
	CTR_CREATE_FUNC(numberDivi, &ctr_number_div, "div:", Number);
	CTR_CREATE_FUNC(numberHiThan, &ctr_number_higherThan, ">", Number);
	CTR_CREATE_FUNC(numberHiEqThan, &ctr_number_higherEqThan, ">=", Number);
	CTR_CREATE_FUNC(numberLoThan, &ctr_number_lowerThan, "<", Number);
	CTR_CREATE_FUNC(numberLoEqThan, &ctr_number_lowerEqThan, "<=", Number);
	CTR_CREATE_FUNC(numberEq, &ctr_number_eq, "==", Number);
	CTR_CREATE_FUNC(numberNeq, &ctr_number_neq, "!=", Number);
	CTR_CREATE_FUNC(numberFactorial, &ctr_number_factorial, "factorial", Number);
	CTR_CREATE_FUNC(numberBetween, &ctr_number_between, "between:and:", Number);
	Number->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(TextString, "String", "[String]", 8, OTSTRING);
	CTR_CREATE_FUNC(stringPrintBytes, &ctr_string_printbytes, "printBytes", TextString);
	CTR_CREATE_FUNC(stringBytes, &ctr_string_bytes, "bytes", TextString);
	CTR_CREATE_FUNC(stringLength, &ctr_string_length, "length", TextString);
	CTR_CREATE_FUNC(stringFromTo, &ctr_string_fromto, "from:to:", TextString);
	CTR_CREATE_FUNC(stringConcat, &ctr_string_concat, "+", TextString);
	CTR_CREATE_FUNC(stringEq, &ctr_string_eq, "==", TextString);
	TextString->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(CBlock, "CodeBlock", "[Code]", 6, OTBLOCK);
	CTR_CREATE_FUNC(blockRun, &ctr_block_runIt, "run", CBlock);
	CBlock->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(BoolX, "Boolean", "False", 5, OTBOOL);
	CTR_CREATE_FUNC(ifTrue, &ctr_bool_iftrue, "ifTrue:", BoolX);
	CTR_CREATE_FUNC(ifFalse, &ctr_bool_ifFalse, "ifFalse:", BoolX);
	CTR_CREATE_FUNC(boolOpposite, &ctr_bool_opposite, "opposite", BoolX);
	CTR_CREATE_FUNC(boolAND, &ctr_bool_and, "&&", BoolX);
	CTR_CREATE_FUNC(boolOR, &ctr_bool_or, "||", BoolX);
	BoolX->mark = 2;
	
	CTR_CREATE_OBJECT_TYPE(Nil, "Nil", "Nil", 3, OTNIL);
	CTR_CREATE_FUNC(isNil, &ctr_nil_isnil, "isNil", Nil);
	Nil->mark = 2;
}

obj* ctr_send_message(obj* receiverObject, char* message, args* argumentList) {
	obj* methodObject = NULL;
	obj* searchObject = receiverObject;
	while(!methodObject) {
		HASH_FIND_STR(searchObject->methods, message, methodObject);
		if (methodObject) break;
		if (!searchObject->link) {
			break;
		}
		searchObject = searchObject->link;
	}
	
	if (!methodObject) {
		printf("Object will not respond to: %s\n", message);
		exit(1);
	}
	obj* result;
	if (methodObject->type == OTNATFUNC) {
		obj* (*funct)(obj* receiverObject, args* argumentList);
		funct = (void*) methodObject->value;
		result = (obj*) funct(receiverObject, argumentList);
	}
	
	if (methodObject->type == OTBLOCK) {
		//important! for messages to 'me', adjust the 'my' scope to the receiver itself.
		if (strcmp(receiverObject->name,"me")==0) {
			receiverObject = receiverObject->link;
		}
		result = ctr_block_run(methodObject, argumentList, receiverObject);
	}
	
	return result;
}

obj* ctr_assign_value(char* name, obj* o) {
	obj* object = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(object);
	object->properties = o->properties;
    object->methods = o->methods;
    object->type = o->type;
    object->block = o->block;
    object->link = o->link;
    object->value = calloc(o->vlen, sizeof(char));
    object->vlen = o->vlen;
	 strncpy(object->value, o->value, o->vlen);
    object->name = name;
	ctr_set(object);
	return object;
}

obj* ctr_assign_value_to_my(char* name, obj* o) {
	obj* object = CTR_CREATE_OBJECT();
	CTR_REGISTER_OBJECT(object);
	object->properties = o->properties;
    object->methods = o->methods;
    object->type = o->type;
    object->block = o->block;
    object->link = o->link;
    object->value = calloc(o->vlen, sizeof(char));
    object->vlen = o->vlen;
	 strncpy(object->value, o->value, o->vlen);
    object->name = name;
    obj* my = ctr_find("me");
    obj* foundObject = CTR_CREATE_OBJECT();
	HASH_FIND_STR(my->link->properties, object->name, foundObject);
	if (foundObject) {
		HASH_DELETE(hh, my->link->properties, foundObject);
	}
	
	HASH_ADD_KEYPTR(hh, my->link->properties, name, strlen(name), object);
	return object;
}
