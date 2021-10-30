#include "CodeService/FormatElement/LongExpressionLayoutElement.h"

LongExpressionLayoutElement::LongExpressionLayoutElement()
	: _hasIndent(false)
{
}

FormatElementType LongExpressionLayoutElement::GetType()
{
	return FormatElementType::LongExpressionLayoutElement;
}

void LongExpressionLayoutElement::Serialize(FormatContext& ctx, int position, FormatElement& parent)
{
	SerializeSubExpression(ctx, *this);
	if (_hasIndent)
	{
		ctx.RecoverIndent();
	}
}

void LongExpressionLayoutElement::SerializeSubExpression(FormatContext& ctx, FormatElement& parent)
{
	auto& children = parent.GetChildren();
	for (int i = 0; i < static_cast<int>(children.size()); i++)
	{
		auto child = children[i];

		if (child->GetType() == FormatElementType::SubExpressionElement)
		{
			SerializeSubExpression(ctx, *child);
		}
		else
		{
			child->Serialize(ctx, i, parent);
		}
		if (child->GetType() == FormatElementType::KeepElement)
		{
			if (ctx.GetCharacterCount() == 0 && !_hasIndent)
			{
				_hasIndent = true;
				ctx.AddIndent();
			}
		}
	}
}
